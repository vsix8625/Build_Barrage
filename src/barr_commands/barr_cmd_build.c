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
#include "olmos.h"
#include "olmos_ast.h"

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
        if (barr_mkdir("build/bin"))
        {
            BARR_errlog("Failed to create bin directory");
            return 1;
        }
        if (barr_mkdir("build/obj"))
        {
            BARR_errlog("Failed to create obj directory");
            return 1;
        }
    }

    bool dry_run_compile = false;
    bool is_batch_build = false;

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
        else
        {
            BARR_warnlog("Invalid option for build command: %s", cmd);
        }
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
    OLM_eval_node(olmos_ast);

    // example retrieving variables from Barrfile
    const char *olmos_cflags = OLM_get_var("cflags");
    if (!olmos_cflags)
    {
        olmos_cflags = "-Wall -Wextra -Werror -g";
    }
    const char *olmos_defines = OLM_get_var("defines");
    if (!olmos_defines)
    {
        olmos_defines = "-DDEBUG";
    }

    //----------------------------------------------------------------------------------------------------
    // Begin build

    const char *scan_dir = BARR_getcwd();

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

    // object list to use in LINK STAGE
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

    BARR_source_list_scan_dir(&sources, scan_dir);
    BARR_header_list_scan_dir(&headers, scan_dir, &inc_dir_list);

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
            BARR_create_batches(&sources, &batch_list);
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

    const char *compiler = OLM_get_var("compiler");
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

    const char *out_dir = OLM_get_var("out_dir");
    if (!out_dir)
    {
        out_dir = "build/obj";
    }

    const char *build_type = OLM_get_var("build_type");
    if (!build_type)
    {
        build_type = "debug";
    }
    bool debug_build = (build_type && BARR_strmatch(build_type, "debug"));

    char build_dir_path[BARR_BUF_SIZE_1024];
    snprintf(build_dir_path, sizeof(build_dir_path), "build/bin/%s", build_type);

    if (!BARR_isdir(build_dir_path))
    {
        if (barr_mkdir(build_dir_path))
        {
            BARR_warnlog("%s(): failed to create %s", __func__, build_dir_path);
        }
    }

    char **flags = NULL;
    if (debug_build)
    {
        const char *cflags = OLM_get_var("cflags");
        if (!cflags || !cflags[0])
        {
            cflags = "-Wall -Wextra -Werror -g";
        }
        flags = BARR_tokenize_string(cflags);
    }
    else
    {
        const char *cflags_release = OLM_get_var("cflags_release");
        if (!cflags_release || !cflags_release[0])
        {
            cflags_release = "-O3 -Wall";
        }
        flags = BARR_tokenize_string(cflags_release);
    }

    // prepare includes
    const char *includes_raw[] = {"-Iinc", "-Iinclude", "-Isrc", NULL};
    const char **includes = NULL;

    const char *olmos_includes_raw = OLM_get_var("includes");
    if (olmos_includes_raw && olmos_includes_raw[0] != '\0')
    {
        includes = (const char **) BARR_tokenize_string(olmos_includes_raw);
    }
    else
    {
        size_t orig_count = 0;
        while (includes_raw[orig_count])
        {
            orig_count++;
        }

        size_t total = orig_count + inc_dir_list.count + 1;  // +1 for NULL terminator
        const char **includes_combined = BARR_gc_calloc(total, sizeof(char *));

        for (size_t i = 0; i < orig_count; ++i)
        {
            includes_combined[i] = includes_raw[i];  // copy original flags
        }

        for (size_t i = 0; i < inc_dir_list.count; ++i)
        {
            char *buf = BARR_gc_alloc(BARR_PATH_MAX + 3);  // "-I" + path + '\0'
            snprintf(buf, BARR_PATH_MAX + 3, "-I%s", inc_dir_list.entries[i]);
            includes_combined[orig_count + i] = buf;
        }
        includes_combined[total - 1] = NULL;
        includes = includes_combined;
    }

    BARR_List merged_includes;
    BARR_list_init(&merged_includes, 8);

    for (const char **p = includes; p && *p; ++p)
    {
        BARR_list_push(&merged_includes, BARR_gc_strdup(*p));
    }

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
    const char *olm_defines = OLM_get_var("defines");
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

    BARR_BuildProgressCTX progress_ctx = {
        .completed = 0, .total = compile_list.count, .log_mutex = PTHREAD_MUTEX_INITIALIZER};

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

    size_t producer_arena_size = compile_list.count * BARR_BUF_SIZE_4096;
    BARR_Arena producer_arena;
    BARR_arena_init(&producer_arena, producer_arena_size, "producer_arena", 16);

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
        snprintf(job->out_file, sizeof(job->out_file), "build/obj/%s.o", base_with_ext);

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
        if (!BARR_strmatch(job->out_file, "build/obj/main.c.o"))
        {
            (void) BARR_source_list_push(&object_list, job->out_file);
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
        BARR_rmrf("build/batch");
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

    if (!dry_run_compile)
    {
        if (!BARR_isdir("build/bin"))
        {
            if (barr_mkdir("build/bin"))
            {
                BARR_errlog("Failed to create bin directory");
                return 1;
            }
            // TODO: need to change this with config build type from olmos
            if (barr_mkdir("build/bin/debug"))
            {
                BARR_errlog("Failed to create debug directory");
                return 1;
            }
        }

        // out name
        const char *project_name = OLM_get_var("project");
        if (!project_name)
        {
            project_name = "barr_default";
        }

        clock_gettime(CLOCK_MONOTONIC, &arch_start);
        // archive stage
        barr_i32 archive_ret = BARR_archive_stage(&object_list, "build/libbarr.a");
        if (archive_ret != 0)
        {
            BARR_errlog("Archive stage failed");
        }
        clock_gettime(CLOCK_MONOTONIC, &arch_end);

        const char *target_type = OLM_get_var("target_type");
        if (!target_type || !target_type[0])
        {
            target_type = "executable";
        }

        char lib_dir[BARR_PATH_MAX];
        snprintf(lib_dir, sizeof(lib_dir), "build/lib/%s", debug_build ? "debug" : "release");
        barr_mkdir("build/lib");
        barr_mkdir(lib_dir);

        if (BARR_strmatch(target_type, "library"))
        {
            // ----------------------------------------------------------------------------------------------------
            // STATIC & SHARED LIB
            // ----------------------------------------------------------------------------------------------------

            // TODO: we will possibly need to pass pkg_paths here also

            char dest_static[BARR_PATH_MAX * 2];
            snprintf(dest_static, sizeof(dest_static), "%s/lib%s.a", lib_dir, project_name);

            BARR_mv("build/libbarr.a", dest_static);

            char dest_shared[BARR_PATH_MAX * 2];
            snprintf(dest_shared, sizeof(dest_shared), "%s/lib%s.so", lib_dir, project_name);

            BARR_log("Building shared library: %s", dest_shared);

            BARR_List so_args;
            BARR_list_init(&so_args, 16);
            BARR_list_push(&so_args, (char *) resolved_compiler);
            BARR_list_push(&so_args, "-shared");
            BARR_list_push(&so_args, "-fPIC");
            BARR_list_push(&so_args, "-o");
            BARR_list_push(&so_args, dest_shared);

            for (size_t i = 0; i < object_list.count; ++i)
            {
                const char *obj = object_list.entries[i];
                if (!BARR_strmatch(obj, "build/obj/main.c.o"))
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
        }
        else
        {
            BARR_log("Building executable target: %s", project_name);
            clock_gettime(CLOCK_MONOTONIC, &link_start);
            // base linker args
            char *base_link_args[] = {resolved_compiler,
                                      "build/obj/main.c.o",  // main object
                                      "-fuse-ld=lld", "-Wl,--threads=4", "-Lbuild"};

            BARR_LinkArgs *la = BARR_link_args_create();

            // push base link args
            size_t base_la_count = sizeof(base_link_args) / sizeof(base_link_args[0]);
            for (size_t i = 0; i < base_la_count; ++i)
            {
                BARR_link_args_add(la, base_link_args[i]);
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
                        BARR_dbglog("[pkg] libs for package: %s -> %s", pkg_info->name, pkg_info->libs);
                        libs_raw[idx++] = pkg_info->libs;
                    }
                }

                libs_raw[idx] = NULL;

                BARR_dedup_libs_and_add_to_link_args(la, libs_raw);
            }
            else
            {
                BARR_dbglog("No package libs to link — skipping pkg libs stage.");
            }

            BARR_link_args_add(la, "-lbarr");

            const char *lib_paths_raw = OLM_get_var("lib_paths");
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

            const char *user_libs_raw = OLM_get_var("user_libs");
            if (user_libs_raw && user_libs_raw[0])
            {
                char **user_libs = BARR_tokenize_string(user_libs_raw);
                for (char **p = user_libs; p && *p; ++p)
                {
                    BARR_link_args_add(la, *p);
                }
            }
            else
            {
                user_libs_raw = "";
            }

            char output_path[BARR_PATH_MAX];
            snprintf(output_path, sizeof(output_path), "%s/bin/%s/%s", "build", debug_build ? "debug" : "release",
                     project_name);

            BARR_link_args_add(la, "-o");
            BARR_link_args_add(la, output_path);

            // finalize link args
            char **link_args = BARR_link_args_finalize(la);
            if (1)  // TODO: add verbose in future
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
    barr_i64 compile_sec = (barr_i64) (compile_end.tv_sec - compile_start.tv_sec);
    barr_i64 compile_nsec = (barr_i64) (compile_end.tv_nsec - compile_start.tv_nsec);
    if (compile_nsec < 0)
    {
        --compile_sec;
        compile_nsec += 1000000000LL;
    }
    double compile_elapsed = (double) compile_sec + (double) compile_nsec / 1e9;

    barr_i64 arch_sec = (barr_i64) (arch_end.tv_sec - arch_start.tv_sec);
    barr_i64 arch_nsec = (barr_i64) (arch_end.tv_nsec - arch_start.tv_nsec);
    if (arch_nsec < 0)
    {
        --arch_sec;
        arch_nsec += 1000000000LL;
    }
    double arch_elapsed = (double) arch_sec + (double) arch_nsec / 1e9;

    barr_i64 link_sec = (barr_i64) (link_end.tv_sec - link_start.tv_sec);
    barr_i64 link_nsec = (barr_i64) (link_end.tv_nsec - link_start.tv_nsec);
    if (link_nsec < 0)
    {
        --link_sec;
        link_nsec += 1000000000LL;
    }
    double link_elapsed = (double) link_sec + (double) link_nsec / 1e9;

    clock_gettime(CLOCK_MONOTONIC, &build_end);
    barr_i64 build_sec = (barr_i64) (build_end.tv_sec - build_start.tv_sec);
    barr_i64 build_nsec = (barr_i64) (build_end.tv_nsec - build_start.tv_nsec);
    if (build_nsec < 0)
    {
        --build_sec;
        build_nsec += 1000000000LL;
    }
    double elapsed = (double) build_sec + (double) build_nsec / 1e9;

    BARR_log("Time to compile sources: \033[34;1m %.6fs", compile_elapsed);
    BARR_log("Time to archive: \033[34;1m %.6fs", arch_elapsed);
    BARR_log("Time to link: \033[34;1m %.6fs", link_elapsed);
    BARR_log("Time to build: \033[34;1m %.6fs", elapsed);
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
