#include "barr_cmd_build.h"
#include "barr_debug.h"
#include "barr_env.h"
#include "barr_glob_config_keys.h"
#include "barr_glob_config_parser.h"
#include "barr_io.h"
#include "barr_src_list.h"
#include "barr_src_scan.h"
#include "crx.h"
#include "crx_ast.h"

#include <inttypes.h>
#include <libgen.h>
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

    BARR_source_list_scan_dir(&list, scan_dir);

    BARR_HashMap *current_map = BARR_hashmap_create(BARR_BUF_SIZE_8192);
    BARR_source_list_hash(&list, current_map, "-Wall");
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
        bool changed = false;
        if (!current_hash)
        {
            changed = true;
        }
        else if (!cached_hash)
        {
            changed = true;
        }
        else if (memcmp(current_hash, cached_hash, BARR_SHA256_LEN) != 0)
        {
            changed = true;
        }

        if (changed)
        {
            if (!BARR_source_list_push(&compile_list, file))
            {
                BARR_errlog("%s(): failed to push to compile list", __func__);
            }
        }
    }

    // write current hash to cache
    if (!BARR_hashmap_write_cache(current_map, BARR_CACHE_FILE))
    {
        BARR_errlog("%s(): failed to write cache", __func__);
    }

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
    // TODO: this stage will become a compile job with args_ctx probably
    // TODO: add multi thread

    for (size_t i = 0; i < compile_list.count; i++)
    {
        const char *src = compile_list.entries[i];
        char *tmp_src = strdup(src);
        char *base_with_ext = basename(tmp_src);

        char out_file[BARR_BUF_SIZE_2048];
        snprintf(out_file, sizeof(out_file), "build/obj/%s.o", base_with_ext);

        // placeholder args
        char *args[] = {"gcc",     "-c",      (char *) src, "-o",      out_file, "-Wall",
                        "-Werror", "-Wextra", "-g",         "-DDEBUG", NULL

        };

        if (BARR_run_process(args[0], args, false) != 0)
        {
            BARR_errlog("Compilation failed for: %s", src);

            // cleanup
            CRX_close();
            BARR_destroy_source_list(&list);
            BARR_destroy_source_list(&compile_list);
            if (cached_map)
            {
                BARR_destroy_hashmap(cached_map);
            }
            BARR_destroy_hashmap(current_map);
            rc_table->destroy(rc_table);
            return 1;
        }
        printf("\r[%zu/%zu] >>>> %s", i + 1, list.count, src);
        fflush(stdout);
        free(tmp_src);
    }
    printf("\n");

    //----------------------------------------------------------------------------------------------------
    // link stage placeholder

    if (!BARR_isdir("build/bin"))
    {
        if (barr_mkdir("build/bin"))
        {
            BARR_errlog("Failed to create bin directory");
            return 1;
        }
        // TODO: need to change this with config build type
        if (barr_mkdir("build/bin/debug"))
        {
            BARR_errlog("Failed to create debug directory");
            return 1;
        }
    }

    // NOTE: either we go with batch link , static archives or incremental link?
    // char *link_args[] = {"gcc", "build/obj/*.o", "-o", "build/bin/debug/barr_placeholder", NULL};
    // BARR_run_process(link_args[0], link_args, true);

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

    // cleanup
    CRX_close();
    BARR_destroy_source_list(&list);
    BARR_destroy_source_list(&compile_list);
    if (cached_map)
    {
        BARR_destroy_hashmap(cached_map);
    }
    BARR_destroy_hashmap(current_map);
    rc_table->destroy(rc_table);
    return 0;
}
