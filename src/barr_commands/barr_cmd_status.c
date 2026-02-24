#include "barr_cmd_status.h"
#include "barr_gc.h"
#include "barr_io.h"
#include "olmos.h"
#include "olmos_variables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static inline const char *barr_fmt_time_ago(time_t past_tm_stamp)
{
    time_t now  = time(NULL);
    double diff = difftime(now, past_tm_stamp);

    static char buf[BARR_BUF_SIZE_64];

    if (diff < 0)
    {
        return "In the future";
    }

    if (diff < 60)
    {
        snprintf(buf, sizeof(buf), "%.0f sec ago", diff);
    }
    else if (diff < 3600)
    {
        barr_i32 mins = (barr_i32) (diff / 60);
        snprintf(buf, sizeof(buf), "%d min%s ago", mins, mins == 1 ? "" : "s");
    }
    else if (diff < 86400)
    {
        barr_i32 hours = (barr_i32) (diff / 3600);
        snprintf(buf, sizeof(buf), "%d hour%s ago", hours, hours == 1 ? "" : "s");
    }
    else if (diff < 2592000)
    {  // ~30 days
        barr_i32 days = (barr_i32) (diff / 86400);
        snprintf(buf, sizeof(buf), "%d day%s ago", days, days == 1 ? "" : "s");
    }
    else if (diff < 31536000)
    {  // ~365 days
        barr_i32 months = (barr_i32) (diff / 2592000);
        snprintf(buf, sizeof(buf), "%d month%s ago", months, months == 1 ? "" : "s");
    }
    else
    {
        barr_i32 years = (barr_i32) (diff / 31536000);
        snprintf(buf, sizeof(buf), "%d year%s ago", years, years == 1 ? "" : "s");
    }

    return buf;
}

barr_i32 BARR_cmd_status(barr_i32 argc, char **argv)
{
    barr_i32 result = 0;

    BARR_printf(
        "--------------------------------------------------------------------------------------\n");

    BARR_InitStatus res = BARR_check_initialized();

    switch (res)
    {
        case BARR_INIT_NOT_FOUND:
        {
            // BARR_is_initialized() already prints logs if you call it
            BARR_init();  // will log errors/warnings
            result = 1;
            break;
        }

        case BARR_INIT_LOCK_NEWER:
        {
            BARR_init();  // logs error about newer lock
            result = 1;
            break;
        }

        case BARR_INIT_LOCK_OLDER:
        {
            BARR_init();  // logs warning
            break;
        }

        case BARR_INIT_OK:
        {
            BARR_printf("Build Barrage version %d.%d.%d | Working directory: %s\n",
                        BARR_VERSION_MAJOR,
                        BARR_VERSION_MINOR,
                        BARR_VERSION_PATCH,
                        BARR_getcwd());
            break;
        }

        default:
        {
            BARR_errlog("Unknown initialization state!");
            result = 1;
            break;
        }
    }

    //---------------------------------------------------
    // init olmos and parse Barrfile variables

    OLM_init();
    OLM_Node *root = OLM_parse_file("Barrfile");
    if (root == NULL)
    {
        BARR_errlog("%s: failed to parse Barrfile", __func__);
        return 1;
    }

    OLM_parse_vars(root);

    //---------------------------------------------------

    char        obj_dir[BARR_PATH_MAX];
    const char *out_dir_var = OLM_get_var(OLM_VAR_OUT_DIR);

    if (out_dir_var == NULL)
    {
        BARR_warnlog("%s: Failed to parse Barrfile 'OUT_DIR': fallback to build/debug/obj",
                     __func__);
        snprintf(obj_dir, sizeof(obj_dir), "build/debug/obj");
    }
    else
    {
        snprintf(obj_dir, sizeof(obj_dir), "%s/obj", out_dir_var);
    }

    char        target_name[BARR_PATH_MAX];
    const char *target = OLM_get_var(OLM_VAR_TARGET);

    if (target == NULL)
    {
        BARR_warnlog(
            "%s: Failed to parse Barrfile 'TARGET': fallback to %s", __func__, basename(obj_dir));
        snprintf(target_name, sizeof(target_name), "%s", obj_dir);
    }
    else
    {
        snprintf(target_name, sizeof(target_name), "%s", target);
    }

    BARR_List obj_list;
    BARR_List src_list;

    BARR_list_init(&obj_list, 64);
    BARR_list_init(&src_list, 64);

    // gen obj list
    BARR_object_files_scan(&obj_list, obj_dir);

    // gen src_list
    FILE *fp = fopen(BARR_DATA_SOURCE_FILES_LOG, "r");
    if (fp == NULL)
    {
        BARR_errlog("Failed to read: %s", BARR_DATA_SOURCE_FILES_LOG);
        return 0;
    }
    else
    {
        char line[BARR_PATH_MAX];
        while (fgets(line, sizeof(line), fp))
        {
            line[strcspn(line, "\r\n")] = 0;
            BARR_list_push(&src_list, BARR_gc_strdup(line));
        }
        fclose(fp);
    }

    BARR_printf("Status for (%s):\n\n", target_name);

    bool print_all = false;
    for (barr_i32 i = 0; i < argc; ++i)
    {
        char *opt = argv[i];
        if (BARR_strmatch(opt, "--all"))
        {
            print_all = true;
        }

        if (BARR_strmatch(opt, "--vars"))
        {
            printf("\t========================================\n");
            printf("\t%-10s%s\n", " ", "BARRFILE VARIABLES");
            printf("\t========================================\n");
            OLM_print_all_vars();
        }

        if (BARR_strmatch(opt, "--binfo"))
        {
            printf("========================================\n");
            printf("%-10s%s\n", " ", "BUILD INFO");
            printf("========================================\n");
            if (BARR_isfile(BARR_DATA_BUILD_INFO_PATH))
            {
                printf("%s\n", BARR_file_read(BARR_DATA_BUILD_INFO_PATH));
            }
        }
    }

    size_t total      = obj_list.count;
    size_t modified   = 0;
    size_t unknown    = 0;
    size_t up_to_date = 0;

    printf("\n\t=== FILE STATUS ===\n");
    for (size_t i = 0; i < obj_list.count; i++)
    {
        const char *obj_file = (const char *) obj_list.items[i];
        const char *obj_base = basename(obj_file);

        bool found = false;
        for (size_t j = 0; j < src_list.count; j++)
        {
            const char *src_file = (const char *) src_list.items[j];
            const char *src_base = basename(src_file);

            size_t len = strlen(src_base);
            if (len + 2 == strlen(obj_base) && strncmp(src_base, obj_base, len) == 0)
            {
                found = true;

                bool changed = BARR_is_newer(src_file, obj_file);
                if (print_all)
                {
                    printf("\t%s -> %s\t%s\n",
                           src_base,
                           obj_base,
                           changed ? "\033[31;1mmodified\033[0m" : "");
                }

                if (changed)
                {
                    modified++;
                    if (!print_all)
                    {
                        printf("\t%s -> %s\t%s\n", src_base, obj_base, "\033[31;1mmodified\033[0m");
                    }
                }
                else
                {
                    up_to_date++;
                }

                break;
            }
        }

        if (!found)
        {
            printf("\t%s\t\tunknown\n", obj_file);
            unknown++;
        }
    }

    if (!modified && !print_all)
    {
        printf("\tAll files are up to date\n");
    }

    BARR_printf("\n--------------------------------------------------------------------------------"
                "------\n");
    printf("[summary]: %zu total, %zu modified, %zu unknown, %zu up-to-date\n",
           total,
           modified,
           unknown,
           up_to_date);

    char *timestamp_str = BARR_get_build_info_key(BARR_DATA_BUILD_INFO_PATH, "timestamp");
    if (timestamp_str)
    {
        time_t      past = (time_t) strtoll(timestamp_str, NULL, 10);
        const char *ago  = barr_fmt_time_ago(past);
        BARR_printf("\033[90mLatest build: %s\n\n", ago);
    }

    printf("\033[90mStatus check: Fast timestamp scan (for rapid feedback)\n");
    printf("\033[90mBuild engine: Full content, flags hashing + dependency tracking\n");
    printf("\033[90m  (use \"barr status --all\" to view all files)\n");
    printf("\033[90m  (use \"barr status --vars\" for Barrfile variables)\n\033[0m");
    OLM_close();
    return result;
}
