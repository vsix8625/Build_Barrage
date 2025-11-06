#include "barr_cmd_build.h"
#include "barr_arena.h"
#include "barr_batch_build.h"
#include "barr_build_ctx.h"
#include "barr_cmd_clean.h"
#include "barr_cpu.h"
#include "barr_debug.h"
#include "barr_env.h"
#include "barr_find_package.h"
#include "barr_gc.h"
#include "barr_glob_config_keys.h"
#include "barr_glob_config_parser.h"
#include "barr_hashmap.h"
#include "barr_io.h"
#include "barr_linker.h"
#include "barr_list.h"
#include "barr_package_scan_dir.h"
#include "barr_src_list.h"
#include "barr_src_scan.h"
#include "olmos_ast.h"
#include "olmos_variables.h"

#include <inttypes.h>
#include <libgen.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

barr_i32 BARR_command_build(barr_i32 argc, char **argv)
{
    struct timespec build_start, build_end;
    struct timespec compile_start, compile_end;
    struct timespec arch_start, arch_end;
    struct timespec link_start, link_end;
    clock_gettime(CLOCK_MONOTONIC, &build_start);

    if (!BARR_is_initialized())
    {
        return 1;
    }

    if (!BARR_isdir("build"))
    {
        if (barr_mkdir("build"))
        {
            BARR_errlog("Failed to create build directory");
            return 1;
        }
    }

    bool dry_run_compile = false;
    bool is_batch_build = false;
    bool select_dir = false;
    char *select_dir_path = NULL;

    for (barr_i32 i = 1; i < argc; ++i)
    {
        char *cmd = argv[i];
        if (BARR_strmatch(cmd, "--dry-run"))
        {
            dry_run_compile = true;
        }
        if (BARR_strmatch(cmd, "--turbo"))
        {
            BARR_log("Turbo mode initialized");
            is_batch_build = true;
        }
        if (BARR_strmatch(cmd, "--dir"))
        {
            select_dir = true;
            if (argv[i + 1])
            {
                select_dir_path = BARR_gc_strdup(argv[i + 1]);
                size_t len = strlen(select_dir_path);
                if (len > 0 && select_dir_path[len - 1] == '/')
                {
                    select_dir_path[len - 1] = '\0';
                }
                break;
            }
            else
            {
                BARR_errlog("%s(): --dir options reuires directory path: '%s' is invalid", __func__, argv[i + 1]);
                select_dir = false;
                break;
            }
        }
        else
        {
            BARR_warnlog("Invalid option for build command: %s", cmd);
            break;
        }
    }

    if (select_dir)
    {
        const char *old_dir = BARR_getcwd();
        BARR_log("Switching build context to directory: %s", select_dir_path);

        if (chdir(select_dir_path) != 0)
        {
            BARR_errlog("%s(): failed to change directory to %s", __func__, select_dir_path);
            return 1;
        }

        barr_i32 status = BARR_command_build(0, NULL);
        if (status != 0)
        {
            BARR_errlog("Sub-build failed for '%s'", select_dir_path);
        }

        BARR_log("Switching build context to directory: %s", old_dir);
        if (chdir(old_dir) != 0)
        {
            BARR_errlog("%s(): failed to restore working directory to %s", __func__, old_dir);
            return 1;
        }

        return status;
    }

    bool has_cache = false;
    if (BARR_isfile(BARR_CACHE_FILE))
    {
        has_cache = true;
    }

    //  parse global config
    const char *rc_file = BARR_get_config("file");
    BARR_ConfigTable *rc_table = BARR_config_parse_file(rc_file);

    //----------------------------------------------------------------------------------------------------
    // olmos

    if (!OLM_init())
    {
        BARR_errlog("%s(): failed to initialize olmos", __func__);
        rc_table->destroy(rc_table);
        return 1;
    }

    if (!BARR_isfile(BARR_OLMOS_FILE))
    {
        BARR_errlog("Fatal: barr build command require 'Barrfile' in %s to run", BARR_getcwd());
        rc_table->destroy(rc_table);
        return 1;
    }
    OLM_AST_Node *olmos_ast = OLM_parse_file(BARR_OLMOS_FILE);
    if (!olmos_ast)
    {
        BARR_errlog("Fatal: failed to parse %s", BARR_OLMOS_FILE);
        rc_table->destroy(rc_table);
        return 1;
    }

    BARR_Arena olm_eval_arena;
    size_t total_nodes = BARR_count_nodes(olmos_ast);
    size_t olm_eval_arena_size = (total_nodes * 2) * sizeof(OLM_AST_Node *);
    BARR_arena_init(&olm_eval_arena, olm_eval_arena_size, "olmos_eval_arena", 32);

    OLM_eval_node(olmos_ast, &olm_eval_arena);
    BARR_destroy_arena(&olm_eval_arena);

    // example retrieving variables from Barrfile
    const char *olmos_cflags = OLM_get_var(OLM_VAR_CFLAGS);
    if (!olmos_cflags)
    {
        olmos_cflags = "-Wall -Wextra -Werror -g";
    }
    const char *olmos_defines = OLM_get_var(OLM_VAR_DEFINES);
    if (!olmos_defines)
    {
        olmos_defines = "-DDEBUG";
    }

    //----------------------------------------------------------------------------------------------------
    // Begin build

    const char *root_dir = BARR_getcwd();

    // generic list for full scan
    BARR_SourceList sources;
    if (!BARR_source_list_init(&sources, BARR_SOURCE_LIST_INITIAL_FILES))
    {
        BARR_errlog("%s(): failed to initialize source list.", __func__);
        OLM_close();
        rc_table->destroy(rc_table);
        return 1;
    }

    BARR_SourceList headers;
    if (!BARR_source_list_init(&headers, BARR_SOURCE_LIST_INITIAL_FILES))
    {
        BARR_errlog("%s(): failed to initialize header list.", __func__);
        OLM_close();
        rc_table->destroy(rc_table);
        return 1;
    }

    BARR_SourceList inc_dir_list;
    if (!BARR_source_list_init(&inc_dir_list, BARR_BUF_SIZE_32))
    {
        BARR_errlog("%s(): failed to initialize include dirs list list.", __func__);
        OLM_close();
        BARR_destroy_source_list(&sources);
        BARR_destroy_source_list(&headers);
        rc_table->destroy(rc_table);
        return 1;
    }

    // compile list to push source files for compilation
    BARR_SourceList compile_list;
    if (!BARR_source_list_init(&compile_list, BARR_SOURCE_LIST_INITIAL_FILES))
    {
        BARR_errlog("%s(): failed to initialize compile list.", __func__);
        OLM_close();
        BARR_destroy_source_list(&sources);
        BARR_destroy_source_list(&headers);
        BARR_destroy_source_list(&inc_dir_list);
        rc_table->destroy(rc_table);
        return 1;
    }

    // object list to use in
    BARR_SourceList object_list;
    if (!BARR_source_list_init(&object_list, BARR_SOURCE_LIST_INITIAL_FILES))
    {
        BARR_errlog("%s(): failed to initialize object list.", __func__);
        OLM_close();
        BARR_destroy_source_list(&sources);
        BARR_destroy_source_list(&headers);
        BARR_destroy_source_list(&inc_dir_list);
        BARR_destroy_source_list(&compile_list);
        rc_table->destroy(rc_table);
        return 1;
    }

    //----------------------------------------------------------------------------------------------------
    // scan exclude

    const char *exclude_raw = OLM_get_var(OLM_VAR_EXCLUDE_PATTERNS);
    const char **exclude_tokens = NULL;
    if (exclude_raw && exclude_raw[0] != '\0')
    {
        exclude_tokens = (const char **) BARR_tokenize_string(exclude_raw);
    }
    else
    {
        exclude_tokens = NULL;
    }

    //----------------------------------------------------------------------------------------------------
    // prepare source files

    const char *sources_glob_raw = OLM_get_var(OLM_VAR_GLOB_SOURCES);
    const char *sources_raw = OLM_get_var(OLM_VAR_SOURCES);

    bool has_manual_sources = sources_raw && sources_raw[0] != '\0';
    bool has_manual_sources_glob = sources_glob_raw && sources_glob_raw[0] != '\0';

    if (has_manual_sources_glob)
    {
        const char **sources_tokens = (const char **) BARR_tokenize_string(sources_glob_raw);
        for (const char **p = sources_tokens; p && *p; ++p)
        {
            BARR_source_list_scan_dir(&sources, *p, exclude_tokens);
        }
    }
    else
    {
        BARR_source_list_scan_dir(&sources, root_dir, exclude_tokens);
    }

    if (has_manual_sources)
    {
        const char **src_files = (const char **) BARR_tokenize_string(sources_raw);
        for (const char **f = src_files; f && *f; ++f)
        {
            if (!BARR_source_list_push(&sources, *f))
            {
                BARR_warnlog("Failed to push source file '%s'", *f);
            }
        }
    }

    //----------------------------------------------------------------------------------------------------

    BARR_header_list_scan_dir(&headers, root_dir, &inc_dir_list, exclude_tokens);

    const char *build_type = OLM_get_var(OLM_VAR_BUILD_TYPE);
    if (!build_type)
    {
        OLM_store_var(OLM_VAR_BUILD_TYPE, "debug");
        build_type = "debug";
    }
    bool debug_build = (build_type && BARR_strmatch(build_type, "debug"));

    // get the out dir from Barrfile
    const char *out_dir = OLM_get_var(OLM_VAR_OUT_DIR);
    if (out_dir == NULL)
    {
        char tmp_out_dir[BARR_PATH_MAX];
        snprintf(tmp_out_dir, BARR_PATH_MAX, "%s/build/debug", root_dir);
        out_dir = tmp_out_dir;
    }

    if (!BARR_isdir(out_dir))
    {
        if (BARR_mkdir_p(out_dir))
        {
            BARR_errlog("Failed to create out directory");
            OLM_close();
            BARR_destroy_source_list(&sources);
            BARR_destroy_source_list(&headers);
            BARR_destroy_source_list(&inc_dir_list);
            BARR_destroy_source_list(&compile_list);
            rc_table->destroy(rc_table);
            return 1;
        }
    }

    char obj_dir_buf[BARR_PATH_MAX + 32];
    snprintf(obj_dir_buf, sizeof(obj_dir_buf), "%s/obj", out_dir);
    BARR_mkdir_p(obj_dir_buf);

    BARR_SourceList batch_list = {0};
    if (is_batch_build)
    {
        if (!BARR_source_list_init(&batch_list, BARR_SOURCE_LIST_INITIAL_FILES))
        {
            BARR_warnlog("%s(): failed to initialize batch list, disabling batch mode", __func__);
            is_batch_build = false;
        }
        else
        {
            char batch_dir[BARR_PATH_MAX];
            snprintf(batch_dir, BARR_PATH_MAX, "%s/build/%s/batch", root_dir, build_type);

            BARR_create_batches(&sources, &batch_list, batch_dir);
            sources = batch_list;

            BARR_destroy_source_list(&batch_list);
            memset(&batch_list, 0, sizeof(BARR_SourceList));
        }
    }

    BARR_HashMap *current_map = BARR_hashmap_create(sources.count + (sources.count >> 2));

    BARR_InfoCPU cpu = {0};
    BARR_get_cpu_info(&cpu);
    BARR_log("[cpu]: model='%s', cores=%d, threads=%d, freq=%.2f MHz, cache=%.2f MB",
             cpu.model[0] ? cpu.model : "unknown", cpu.cores, cpu.threads, cpu.mhz,
             (double) cpu.cache_size / (1024.0 * 1024.0));

    // Create thread pool
    barr_i32 n_threads = cpu.threads;
    BARR_ThreadPool *pool = BARR_thread_pool_create(n_threads);

    const char *olmos_cflags_cp = BARR_gc_strdup(olmos_cflags);
    BARR_source_list_hash_mt(&sources, &headers, current_map, olmos_cflags_cp, pool);
    BARR_hashmap_debug(current_map);

    BARR_HashMap *cached_map = NULL;
    if (has_cache)
    {
        BARR_log("Found cache file");
        cached_map = BARR_hashmap_read_cache(BARR_CACHE_FILE);
    }

    for (size_t i = 0; i < sources.count; i++)
    {
        const char *file = sources.entries[i];
        const barr_u8 *current_hash = BARR_hashmap_get(current_map, file);
        const barr_u8 *cached_hash = cached_map ? BARR_hashmap_get(cached_map, file) : NULL;

        // Only compare if both hashes are valid
        bool changed = (!current_hash) || (!cached_hash) || (memcmp(current_hash, cached_hash, BARR_XXHASH_LEN) != 0);

        if (changed)
        {
            (void) BARR_source_list_push(&compile_list, file);
        }
    }

    // info logs
    if (compile_list.count > 0)
    {
        BARR_log("Unchanged files: %zu", sources.count - compile_list.count);
        BARR_log("Files to compile: %zu", compile_list.count);
    }
    else
    {
        if (sources.count == 0)
        {
            BARR_warnlog("Nothing to compile, found 0 source files!");
            goto exit;
        }
        else
        {
            BARR_log("\033[35;1mNothing to compile, all files are up-to-date!");
            goto exit;
        }
    }

    //----------------------------------------------------------------------------------------------------
    // Find packages

    BARR_List pkg_list;
    BARR_list_init(&pkg_list, 4);

    for (size_t i = 0; i < olmos_ast->child_count; i++)
    {
        OLM_AST_Node *node = olmos_ast->children[i];

        if (node->type == OLM_NODE_FN_CALL && BARR_strmatch(node->name, "find_package"))
        {
            char **pkgs = BARR_tokenize_string(node->args[0]);
            for (char **p = pkgs; p && *p; ++p)
            {
                BARR_PackageInfo *pkg_info = BARR_gc_alloc(sizeof(BARR_PackageInfo));
                memset(pkg_info, 0, sizeof(*pkg_info));

                if (!BARR_find_package(*p, pkg_info, 0, NULL))
                {
                    BARR_warnlog("Package %s not found", *p);
                    continue;
                }

                if (!pkg_info->cflags)
                {
                    pkg_info->cflags = BARR_gc_strdup("");
                }

                BARR_list_push(&pkg_list, pkg_info);
            }
        }
    }

    //----------------------------------------------------------------------------------------------------
    // compile stage

    const char *compiler = OLM_get_var(OLM_VAR_COMPILER);
    char *resolved_compiler = NULL;
    if (compiler == NULL)
    {
        resolved_compiler = BARR_which("gcc");
        if (!resolved_compiler)
        {
            BARR_errlog("Could not find compiler");
        }
    }
    else if (compiler && compiler[0] != '\0')
    {
        resolved_compiler = BARR_gc_strdup(compiler);
    }
    BARR_log("Compiler: %s", resolved_compiler);

    char build_dir_path[BARR_BUF_SIZE_4096 * 2];
    snprintf(build_dir_path, sizeof(build_dir_path), "%s/bin", out_dir);

    if (!BARR_isdir(build_dir_path))
    {
        if (BARR_mkdir_p(build_dir_path))
        {
            BARR_warnlog("%s(): failed to create %s", __func__, build_dir_path);
        }
    }

    char **flags = NULL;
    if (debug_build)
    {
        const char *cflags = OLM_get_var(OLM_VAR_CFLAGS);
        if (!cflags || !cflags[0])
        {
            cflags = "-Wall -Wextra -Werror -g";
        }
        flags = BARR_tokenize_string(cflags);
    }
    else
    {
        const char *cflags_release = OLM_get_var(OLM_VAR_CFLAGS_RELEASE);
        if (!cflags_release || !cflags_release[0])
        {
            cflags_release = "-O3 -Wall";
        }
        flags = BARR_tokenize_string(cflags_release);
    }

    // prepare includes
    BARR_List merged_includes;
    BARR_list_init(&merged_includes, 8);

    const char *includes_raw[] = {"-Iinc", "-Iinclude", "-Isrc", NULL};
    const char **includes = NULL;

    const char *olmos_includes_raw = OLM_get_var(OLM_VAR_INCLUDES);
    const char *auto_inc_mode = OLM_get_var(OLM_VAR_AUTO_INC_DISCOVERY);
    bool has_manual_includes = olmos_includes_raw && olmos_includes_raw[0] != '\0';

    enum
    {
        BARR_AUTO_INC_MODE_ON = 0,
        BARR_AUTO_INC_MODE_OFF,
        BARR_AUTO_INC_MODE_APPEND,
    } mode = BARR_AUTO_INC_MODE_ON;

    if (auto_inc_mode)
    {
        if (BARR_strmatch(auto_inc_mode, "off"))
        {
            mode = BARR_AUTO_INC_MODE_OFF;
        }
        else if (BARR_strmatch(auto_inc_mode, "append"))
        {
            mode = BARR_AUTO_INC_MODE_APPEND;
        }
        else if (BARR_strmatch(auto_inc_mode, "on"))
        {
            mode = BARR_AUTO_INC_MODE_ON;
        }
        else
        {
            BARR_warnlog("Unknown auto_include_discovery value '%s', using default (on)", auto_inc_mode);
        }
    }
    else if (has_manual_includes)
    {
        mode = BARR_AUTO_INC_MODE_OFF;
        BARR_log("auto_include_discovery not set, disabling auto-scan (manual includes detected)");
    }

    if (has_manual_includes && (mode == BARR_AUTO_INC_MODE_OFF || mode == BARR_AUTO_INC_MODE_APPEND))
    {
        const char **manual = (const char **) BARR_tokenize_string(olmos_includes_raw);
        for (const char **p = manual; p && *p; ++p)
        {
            BARR_list_push(&merged_includes, BARR_gc_strdup(*p));
        }
    }

    if (mode == BARR_AUTO_INC_MODE_ON || mode == BARR_AUTO_INC_MODE_APPEND)
    {
        for (const char **p = includes_raw; p && *p; ++p)
        {
            BARR_list_push(&merged_includes, BARR_gc_strdup(*p));
        }

        for (size_t i = 0; i < inc_dir_list.count; ++i)
        {
            char buf[BARR_PATH_MAX + 3];
            snprintf(buf, sizeof(buf), "-I%s", inc_dir_list.entries[i]);
            BARR_list_push(&merged_includes, BARR_gc_strdup(buf));
        }
    }
    BARR_list_push(&merged_includes, NULL);

    includes = (const char **) merged_includes.items;

    switch (mode)
    {
        case BARR_AUTO_INC_MODE_OFF:
            BARR_log("include discovery: manual only (auto-scan disabled)");
            break;
        case BARR_AUTO_INC_MODE_APPEND:
            BARR_log("include discovery: append mode (manual + auto)");
            break;
        case BARR_AUTO_INC_MODE_ON:
            BARR_log("include discovery: auto mode");
            break;
    }

    //----------------------------------------

    for (size_t i = 0; i < pkg_list.count; ++i)
    {
        BARR_PackageInfo *pkg_info = (BARR_PackageInfo *) pkg_list.items[i];

        if (!pkg_info || !pkg_info->cflags)
        {
            continue;
        }

        char **pkg_flags = BARR_tokenize_string(pkg_info->cflags);
        for (char **f = pkg_flags; f && *f; ++f)
        {
            BARR_list_push(&merged_includes, BARR_gc_strdup(*f));
        }
    }

    BARR_list_push(&merged_includes, NULL);

    includes = BARR_dedup_flags_array((const char **) merged_includes.items);
    // --------------------------------------------------------------------------------------------
    const char *olm_defines = OLM_get_var(OLM_VAR_DEFINES);
    if (!olm_defines)
    {
        olm_defines = "-DDEBUG";
    }
    char **defines = BARR_tokenize_string(olm_defines);

    for (const char **p = includes; p && *p; ++p)
    {
        BARR_log("[%zu]: %s", (size_t) (p - includes), *p);
    }

    // TODO: need to add std flag
    BARR_CompileInfoCTX compile_ctx = {.compiler = resolved_compiler,
                                       .flags = (const char **) flags,
                                       .out_dir = out_dir,
                                       .includes = includes,
                                       .defines = (const char **) defines,
                                       .debug_build = debug_build};

    BARR_BuildProgressCTX progress_ctx = {.completed = 0,
                                          .total = compile_list.count,
                                          .log_mutex = PTHREAD_MUTEX_INITIALIZER,
                                          .ccmds_json_entries_list = NULL};

    compile_ctx.gen_compile_cmds = false;
    const char *olm_ccmds = OLM_get_var(OLM_VAR_GEN_CCMDS);
    if (olm_ccmds != NULL)
    {
        if (BARR_strmatch(olm_ccmds, "yes"))
        {
            compile_ctx.gen_compile_cmds = true;
            progress_ctx.ccmds_json_entries_list = BARR_gc_alloc(sizeof(BARR_List));
            BARR_list_init(progress_ctx.ccmds_json_entries_list, compile_list.count);
        }
    }

    // get the precompile file from olmos
    const char *precompile_file = "src/common.h";
    compile_ctx.pch_file = precompile_file;

    if (BARR_isfile(compile_ctx.pch_file))
    {
        char out_pch[BARR_PATH_MAX];
        snprintf(out_pch, sizeof(out_pch), "%s/%s.pch", compile_ctx.out_dir, basename((char *) compile_ctx.pch_file));

        if (BARR_compile_pch(&compile_ctx))
        {
            BARR_errlog("%s(): failed to precompile header '%s'", __func__, compile_ctx.pch_file);
            return 1;
        }
    }

    size_t producer_arena_size = compile_list.count * (BARR_BUF_SIZE_8192 + BARR_BUF_SIZE_1024);
    BARR_Arena producer_arena;
    BARR_arena_init(&producer_arena, producer_arena_size, "producer_arena", 16);

    snprintf(compile_ctx.ccmds_path, sizeof(compile_ctx.ccmds_path), "%s/compile_commands.json", out_dir);
    BARR_dbglog("COMPILE_CMDS_PATH: %s", compile_ctx.ccmds_path);

    clock_gettime(CLOCK_MONOTONIC, &compile_start);
    // job producer
    for (size_t i = 0; i < compile_list.count; i++)
    {
        const char *src = compile_list.entries[i];

        BARR_CompileJob *job = (BARR_CompileJob *) BARR_arena_alloc(&producer_arena, sizeof(BARR_CompileJob));
        if (job == NULL)
        {
            BARR_errlog("%s(): failed to allocate memory for compile job", __func__);
            continue;
        }

        job->src = BARR_arena_strdup(&producer_arena, BARR_gc_strdup(src));
        job->ctx = &compile_ctx;
        job->progress_ctx = &progress_ctx;
        job->dry_run = dry_run_compile;

        char *tmp_src = BARR_arena_strdup(&producer_arena, BARR_gc_strdup(src));
        char *base_with_ext = basename(tmp_src);
        // root_dir + out_dir
        snprintf(job->out_file, sizeof(job->out_file), "%s/%s.o", obj_dir_buf, base_with_ext);

        // add to pool for compilation stage
        if (!BARR_thread_pool_add(pool, BARR_compile_job, job))
        {
            BARR_errlog("%s() failed to add to thread pool", __func__);

            // cleanup
            BARR_destroy_arena(&producer_arena);
            BARR_destroy_thread_pool(pool);
            OLM_close();
            BARR_destroy_source_list(&sources);
            BARR_destroy_source_list(&headers);
            BARR_destroy_source_list(&inc_dir_list);
            BARR_destroy_source_list(&compile_list);
            BARR_destroy_source_list(&object_list);
            if (cached_map)
            {
                BARR_destroy_hashmap(cached_map);
            }
            BARR_destroy_hashmap(current_map);
            rc_table->destroy(rc_table);
            return 1;
        }

        // keep the main.c.o out of the archive
        char tmp_main_co[BARR_PATH_MAX * 2];
        snprintf(tmp_main_co, sizeof(tmp_main_co), "%s/obj/main.c.o", out_dir);
        if (!BARR_strmatch(job->out_file, tmp_main_co))
        {
            (void) BARR_source_list_push(&object_list, job->out_file);
        }
        else
        {
            BARR_log("Skipping %s from archive", job->out_file);
        }
    }
    BARR_thread_pool_wait(pool);
    printf("\n");
    BARR_destroy_arena(&producer_arena);
    clock_gettime(CLOCK_MONOTONIC, &compile_end);

    if (!progress_ctx.failed && !dry_run_compile)
    {
        // only write new hash if compilation was successful
        if (!BARR_hashmap_write_cache(current_map, BARR_CACHE_FILE))
        {
            printf("\n");
            BARR_errlog("%s(): failed to write cache", __func__);
        }

        if (progress_ctx.ccmds_json_entries_list)
        {
            FILE *f = fopen(compile_ctx.ccmds_path, "w");
            if (f)
            {
                fprintf(f, "[\n");
                for (size_t i = 0; i < progress_ctx.ccmds_json_entries_list->count; i++)
                {
                    BARR_CompileCmdEntry *entry = progress_ctx.ccmds_json_entries_list->items[i];
                    fprintf(f, "  {\n");
                    fprintf(f, "    \"directory\": \"%s\",\n", entry->directory);
                    fprintf(f, "    \"command\": \"%s\",\n", entry->command);
                    fprintf(f, "    \"file\": \"%s\"\n", entry->file);
                    fprintf(f, "  }%s\n", (i + 1 < progress_ctx.ccmds_json_entries_list->count) ? "," : "");
                }
                fprintf(f, "]\n");
                fclose(f);
                BARR_mv(compile_ctx.ccmds_path, "build/compile_commands.json");
            }
        }

        char batch_dir[BARR_PATH_MAX * 2];
        snprintf(batch_dir, sizeof(batch_dir), "%s/batch", out_dir);
        if (BARR_isdir(batch_dir))
        {
            BARR_rmrf(batch_dir);
        }
    }
    else
    {
        printf("\n");
        if (!dry_run_compile)
        {
            BARR_warnlog("%zu compile jobs failed, cache not updated", progress_ctx.failed);
        }
        else
        {
            BARR_log("barr build --dry-run completed");
            if (progress_ctx.failed)
            {
                BARR_log("Failed compiles: \033[31;1m%zu", progress_ctx.failed);
            }
            BARR_warnlog("NOTICE: No object files produced, the build cache will not update, and the linker stage will "
                         "be skipped");
            BARR_warnlog("This check validates compilation only.");
        }
    }

    BARR_printf("\n");

    //----------------------------------------------------------------------------------------------------

    // out name
    const char *target_name = OLM_get_var(OLM_VAR_TARGET);
    if (target_name == NULL)
    {
        target_name = "barr_target";
    }

    char output_path[BARR_PATH_MAX * 2];
    const char *bin_out_path = OLM_get_var(OLM_VAR_BIN_OUT_PATH);
    if (bin_out_path)
    {
        snprintf(output_path, sizeof(output_path), "%s/%s", root_dir, bin_out_path);
    }
    else
    {
        snprintf(output_path, sizeof(output_path), "%s/bin/%s", out_dir, target_name);
    }

    if (!progress_ctx.failed && !dry_run_compile)
    {
        if (!BARR_isdir("build"))
        {
            if (BARR_mkdir_p(out_dir))
            {
                BARR_errlog("Failed to create out_dir directory");
                return 1;
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &arch_start);
        // archive stage
        char archive_out_path[BARR_PATH_MAX * 2];
        snprintf(archive_out_path, sizeof(archive_out_path), "%s/libbarr.a", out_dir);
        BARR_dbglog("ARCHIVE_PATH: %s", archive_out_path);
        barr_i32 archive_ret = BARR_archive_stage(&object_list, archive_out_path);
        if (archive_ret != 0)
        {
            BARR_errlog("Archive stage failed");
        }
        clock_gettime(CLOCK_MONOTONIC, &arch_end);

        const char *target_type = OLM_get_var(OLM_VAR_TARGET_TYPE);
        if (!target_type || !target_type[0])
        {
            target_type = "executable";
        }

        char lib_dir[BARR_PATH_MAX + 256];
        snprintf(lib_dir, sizeof(lib_dir), "%s/lib", out_dir);
        BARR_mkdir_p(lib_dir);

        if (BARR_strmatch(target_type, "library"))
        {
            // ----------------------------------------------------------------------------------------------------
            // STATIC & SHARED LIB
            // ----------------------------------------------------------------------------------------------------

            clock_gettime(CLOCK_MONOTONIC, &link_start);

            char dest_static[BARR_PATH_MAX * 2];
            snprintf(dest_static, sizeof(dest_static), "%s/%s.a", lib_dir, target_name);
            BARR_mv(archive_out_path, dest_static);

            char dest_shared[BARR_PATH_MAX * 2];
            snprintf(dest_shared, sizeof(dest_shared), "%s/%s.so", lib_dir, target_name);

            BARR_log("Building shared library: %s", dest_shared);

            BARR_List so_args;
            BARR_list_init(&so_args, 16);
            BARR_list_push(&so_args, (char *) resolved_compiler);
            BARR_list_push(&so_args, "-fuse-ld=lld");
            BARR_list_push(&so_args, "-shared");
            BARR_list_push(&so_args, "-fPIC");
            BARR_list_push(&so_args, "-o");
            BARR_list_push(&so_args, dest_shared);

            char main_co_buf[BARR_PATH_MAX * 2];
            snprintf(main_co_buf, sizeof(main_co_buf), "%s/obj/main.c.o", out_dir);
            for (size_t i = 0; i < object_list.count; ++i)
            {
                const char *obj = object_list.entries[i];
                if (!BARR_strmatch(obj, main_co_buf))
                {
                    BARR_list_push(&so_args, (char *) obj);
                }
            }
            BARR_list_push(&so_args, NULL);

            char **so_cmd = (char **) so_args.items;
            barr_i32 so_ret = BARR_run_process(so_cmd[0], so_cmd, false);
            if (so_ret != 0)
            {
                BARR_errlog("Shared lib creation failed");
            }
            clock_gettime(CLOCK_MONOTONIC, &link_end);
        }
        else
        {
            // Link stage
            BARR_log("Building executable target: %s", target_name);
            clock_gettime(CLOCK_MONOTONIC, &link_start);

            // base linker args
            char main_co_buf[BARR_PATH_MAX * 2];
            snprintf(main_co_buf, sizeof(main_co_buf), "%s/obj/main.c.o", out_dir);

            const char *linker_fuse = OLM_get_var(OLM_VAR_LINKER);
            char linker_fuse_buf[BARR_BUF_SIZE_32];
            char lld_threads[BARR_BUF_SIZE_32];

            BARR_LinkArgs *la = BARR_link_args_create();

            BARR_link_args_add(la, resolved_compiler);
            BARR_link_args_add(la, main_co_buf);

            char archive_lib_dirpath[BARR_PATH_MAX * 2];
            snprintf(archive_lib_dirpath, sizeof(archive_lib_dirpath), "-L%s", out_dir);
            BARR_link_args_add(la, archive_lib_dirpath);

            if (linker_fuse != NULL && !BARR_strmatch(linker_fuse, "ld"))
            {
                snprintf(linker_fuse_buf, sizeof(linker_fuse_buf), "-fuse-ld=%s", linker_fuse);
                BARR_link_args_add(la, linker_fuse_buf);
                if (BARR_strmatch(linker_fuse, "lld"))
                {
                    snprintf(lld_threads, sizeof(lld_threads), "-Wl,--threads=%d", n_threads);
                    BARR_link_args_add(la, lld_threads);
                }
            }

            size_t lib_count = 0;
            for (size_t i = 0; i < pkg_list.count; ++i)
            {
                BARR_PackageInfo *pkg_info = (BARR_PackageInfo *) pkg_list.items[i];
                if (!pkg_info)
                {
                    continue;
                }
                if (pkg_info->libs && pkg_info->libs[0])
                {
                    lib_count++;
                }
            }

            if (lib_count > 0)
            {
                const char **libs_raw = BARR_gc_calloc(lib_count + 1, sizeof(char *));
                size_t idx = 0;

                for (size_t i = 0; i < pkg_list.count; ++i)
                {
                    BARR_PackageInfo *pkg_info = (BARR_PackageInfo *) pkg_list.items[i];
                    if (!pkg_info)
                    {
                        BARR_warnlog("Null package info at index %zu — skipping", i);
                        continue;
                    }

                    if (pkg_info->libs && pkg_info->libs[0])
                    {
                        libs_raw[idx++] = pkg_info->libs;
                    }
                }

                libs_raw[idx] = NULL;

                BARR_dedup_libs_and_add_to_link_args(la, libs_raw);
            }
            BARR_link_args_add(la, "-lbarr");

            const char *lib_paths_raw = OLM_get_var(OLM_VAR_LIB_PATHS);
            if (lib_paths_raw && lib_paths_raw[0])
            {
                char **lib_paths = BARR_tokenize_string(lib_paths_raw);
                for (char **p = lib_paths; p && *p; ++p)
                {
                    BARR_link_args_add(la, *p);
                }
            }
            else
            {
                lib_paths_raw = "";
            }

            const char *linker_flags_raw = OLM_get_var(OLM_VAR_LINKER_FLAGS);
            if (linker_flags_raw && linker_flags_raw[0])
            {
                char **user_linker_flags = BARR_tokenize_string(linker_flags_raw);
                for (char **p = user_linker_flags; p && *p; ++p)
                {
                    BARR_link_args_add(la, *p);
                }
            }
            else
            {
                linker_flags_raw = "";
            }

            BARR_link_args_add(la, "-o");
            BARR_link_args_add(la, output_path);
            BARR_dbglog("OUPUT_PATH: %s", output_path);

            // finalize link args
            char **link_args = BARR_link_args_finalize(la);
            if (g_barr_verbose)
            {
                for (char **arg = link_args; *arg != NULL; ++arg)
                {
                    BARR_printf("%s ", *arg);
                }
            }
            BARR_printf("\n");
            barr_i32 link_ret = BARR_link_stage(link_args);
            if (link_ret != 0)
            {
                BARR_errlog("Link stage failed");
            }
            clock_gettime(CLOCK_MONOTONIC, &link_end);
            BARR_file_write(".barr/data/last_bin", "%s", output_path);
        }  // executable
    }

exit:
{
    //----------------------------------------------------------------------------------------------------
    // performance
    BARR_log("Time to compile sources: \033[34;1m %s", BARR_fmt_time_elapsed(&compile_start, &compile_end));
    BARR_log("Time to archive: \033[34;1m %s", BARR_fmt_time_elapsed(&arch_start, &arch_end));
    BARR_log("Time to link: \033[34;1m %s", BARR_fmt_time_elapsed(&link_start, &link_end));

    clock_gettime(CLOCK_MONOTONIC, &build_end);
    BARR_log("Time to build: \033[34;1m %s", BARR_fmt_time_elapsed(&build_start, &build_end));
    //----------------------------------------------------------------------------------------------------

    // cleanup
    BARR_destroy_thread_pool(pool);
    OLM_close();
    BARR_destroy_source_list(&sources);
    BARR_destroy_source_list(&headers);
    BARR_destroy_source_list(&inc_dir_list);
    BARR_destroy_source_list(&compile_list);
    BARR_destroy_source_list(&object_list);
    if (cached_map)
    {
        BARR_destroy_hashmap(cached_map);
    }
    BARR_destroy_hashmap(current_map);
    rc_table->destroy(rc_table);
    return 0;
}
}
