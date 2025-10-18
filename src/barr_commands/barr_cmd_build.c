#include "barr_cmd_build.h"
#include "barr_build_ctx.h"
#include "barr_cmd_mode.h"
#include "barr_debug.h"
#include "barr_env.h"
#include "barr_glob_config_keys.h"
#include "barr_glob_config_parser.h"
#include "barr_io.h"
#include "barr_linker.h"
#include "barr_src_list.h"
#include "barr_src_scan.h"
#include "crx.h"
#include "crx_ast.h"

#include <inttypes.h>
#include <libgen.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

barr_i32 BARR_command_build(barr_i32 argc, char **argv)
{
    BARR_VOID(argc);
    BARR_VOID(argv);
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

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
        if (barr_mkdir("build/obj"))
        {
            BARR_errlog("Failed to create obj directory");
            return 1;
        }
    }

    bool dry_run_compile = false;
    for (barr_i32 i = 1; i < argc; ++i)
    {
        char *cmd = argv[i];
        if (BARR_strmatch(cmd, "--dry-run"))
        {
            dry_run_compile = true;
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
    // Crux

    if (!CRX_init())
    {
        BARR_errlog("%s(): failed to initialize crux", __func__);
        rc_table->destroy(rc_table);
        return 1;
    }

    CRX_AST_Node *root = crx_test_ast();
    CRX_eval_node(root);

    //----------------------------------------------------------------------------------------------------
    // Begin build

    const char *scan_dir = BARR_getcwd();

    // generic list for full scan
    BARR_SourceList list;
    if (!BARR_source_list_init(&list, BARR_SOURCE_LIST_INITIAL_FILES))
    {
        BARR_errlog("%s(): failed to initialize source list.", __func__);
        CRX_close();
        rc_table->destroy(rc_table);
        return 1;
    }

    // compile list to push source files for compilation
    BARR_SourceList compile_list;
    if (!BARR_source_list_init(&compile_list, BARR_SOURCE_LIST_INITIAL_FILES))
    {
        BARR_errlog("%s(): failed to initialize compile list.", __func__);
        CRX_close();
        BARR_destroy_source_list(&list);
        rc_table->destroy(rc_table);
        return 1;
    }

    // object list to use in LINK STAGE
    BARR_SourceList object_list;
    if (!BARR_source_list_init(&object_list, BARR_SOURCE_LIST_INITIAL_FILES))
    {
        BARR_errlog("%s(): failed to initialize object list.", __func__);
        CRX_close();
        BARR_destroy_source_list(&list);
        BARR_destroy_source_list(&compile_list);
        rc_table->destroy(rc_table);
        return 1;
    }

    BARR_source_list_scan_dir(&list, scan_dir);

    BARR_HashMap *current_map = BARR_hashmap_create(list.count + (list.count >> 2));
    BARR_dbglog("%s() current map created", __func__);

    // Create thread pool
    barr_i32 cores = BARR_OS_GET_CORES();
    BARR_ThreadPool *pool = BARR_thread_pool_create(cores);

    // cflags we will get them from crux
    BARR_source_list_hash_mt(&list, current_map, "-Wall -Wextra", pool);
    BARR_hashmap_debug(current_map);

    BARR_HashMap *cached_map = NULL;
    if (has_cache)
    {
        BARR_log("Found cache file");
        cached_map = BARR_hashmap_read_cache(BARR_CACHE_FILE);
    }

    for (size_t i = 0; i < list.count; i++)
    {
        const char *file = list.entries[i];
        const barr_u8 *current_hash = BARR_hashmap_get(current_map, file);
        const barr_u8 *cached_hash = cached_map ? BARR_hashmap_get(cached_map, file) : NULL;

        // Only compare if both hashes are valid
        bool changed = (!current_hash) || (!cached_hash) || (memcmp(current_hash, cached_hash, BARR_XXHASH_LEN) != 0);

        if (changed)
        {
            if (!BARR_source_list_push(&compile_list, file))
            {
                BARR_errlog("%s(): failed to push to compile list", __func__);
            }
        }
    }

    // info logs
    if (compile_list.count > 0)
    {
        BARR_log("Unchanged files: %zu", list.count - compile_list.count);
        BARR_log("Files to compile: %zu", compile_list.count);
    }
    else
    {
        BARR_log("Nothing to compile, all files are up-to-date!");
    }

    //----------------------------------------------------------------------------------------------------
    // compile stage

    // placeholder flags
    const char *flags[] = {"-Werror", "-Wextra", "-Wall", "-g", NULL};
    const char *includes[] = {"-Iinc", NULL};
    const char *defines[] = {"-DDEBUG", NULL};

    BARR_CompileInfoCTX compile_ctx = {.compiler = "gcc",
                                       .flags = flags,
                                       .out_dir = "build/obj",
                                       .includes = includes,
                                       .defines = defines,
                                       .debug_build = true};

    BARR_BuildProgressCTX progress_ctx = {
        .completed = 0, .total = compile_list.count, .log_mutex = PTHREAD_MUTEX_INITIALIZER};

    // get the precompile file from crux
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

    for (size_t i = 0; i < compile_list.count; i++)
    {
        const char *src = compile_list.entries[i];
        BARR_CompileJob *job = malloc(sizeof(BARR_CompileJob));
        if (!job)
        {
            BARR_errlog("%s(): failed to allocate compile job", __func__);
            continue;
        }

        job->src = strdup(src);
        job->ctx = &compile_ctx;
        job->progress_ctx = &progress_ctx;
        job->dry_run = dry_run_compile;

        char *tmp_src = strdup(src);
        char *base_with_ext = basename(tmp_src);
        snprintf(job->out_file, sizeof(job->out_file), "build/obj/%s.o", base_with_ext);

        // add to pool for compilation stage
        if (!BARR_thread_pool_add(pool, BARR_compile_job, job))
        {
            BARR_errlog("%s() failed to add to thread pool", __func__);
            free(job->src);
            free(job);

            // cleanup
            BARR_destroy_thread_pool(pool);
            CRX_close();
            BARR_destroy_source_list(&list);
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
            BARR_source_list_push(&object_list, job->out_file);
        }

        free(tmp_src);
    }
    BARR_thread_pool_wait(pool);

    if (!progress_ctx.failed && !dry_run_compile)
    {
        // only write new hash if compilation was successful
        if (!BARR_hashmap_write_cache(current_map, BARR_CACHE_FILE))
        {
            printf("\n");
            BARR_errlog("%s(): failed to write cache", __func__);
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

    if (!dry_run_compile)
    {
        if (!BARR_isdir("build/bin"))
        {
            if (barr_mkdir("build/bin"))
            {
                BARR_errlog("Failed to create bin directory");
                return 1;
            }
            // TODO: need to change this with config build type from crux
            if (barr_mkdir("build/bin/debug"))
            {
                BARR_errlog("Failed to create debug directory");
                return 1;
            }
        }

        // archive stage
        barr_i32 archive_ret = BARR_archive_stage(&object_list, "build/libbarr.a");
        if (archive_ret != 0)
        {
            BARR_errlog("Archive stage failed");
        }

        // link stage
        // out file will be obtain from crux as well build type and ncores maybe
        // linker for -fuse flag etc
        // following link_args are a placeholder

        char *link_args[] = {"gcc",
                             "build/obj/main.c.o",  // main object
                             "-Lbuild",
                             "-lbarr",
                             "-fuse-ld=lld",
                             "-Wl,--threads=4",
                             "-o",
                             "build/bin/debug/barr_placeholder",
                             NULL};
        barr_i32 link_ret = BARR_link_stage(link_args);
        if (link_ret != 0)
        {
            BARR_errlog("Link stage failed");
        }
    }

    //----------------------------------------------------------------------------------------------------
    // CLOCK
    clock_gettime(CLOCK_MONOTONIC, &end);
    barr_i64 sec = (barr_i64) (end.tv_sec - start.tv_sec);
    barr_i64 nsec = (barr_i64) (end.tv_nsec - start.tv_nsec);
    if (nsec < 0)
    {
        --sec;
        nsec += 1000000000LL;
    }
    double elapsed = (double) sec + (double) nsec / 1e9;
    BARR_log("Build completed! | Time: \033[34;1m %.6fs", elapsed);

    //----------------------------------------------------------------------------------------------------
    // will probably remove this stupidity
    // MODES

    if (BARR_is_mode_active("war"))
    {
        BARR_ConfigEntry *root_dir_entry = BARR_config_table_get(rc_table, BARR_GLOB_CONFIG_KEY_ROOT_DIR);
        if (!root_dir_entry)
        {
            BARR_errlog("%s(): failed to get config entry for %s", __func__, BARR_GLOB_CONFIG_KEY_ROOT_DIR);
        }
        const char *barr_install_dir_path = root_dir_entry->value.str_val ? root_dir_entry->value.str_val : "N/A";

        char victory_sound_path[BARR_PATH_MAX];
        snprintf(victory_sound_path, sizeof(victory_sound_path), "%s/%s", barr_install_dir_path,
                 "assets/sounds/machine-gun.mp3");

        char *placeholder_sound_args[] = {"paplay", victory_sound_path, NULL};
        BARR_run_process_BG(placeholder_sound_args[0], placeholder_sound_args);
    }

    //----------------------------------------------------------------------------------------------------

    // cleanup
    BARR_destroy_thread_pool(pool);
    CRX_close();
    BARR_destroy_source_list(&list);
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
