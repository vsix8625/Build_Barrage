#include "barr_cmd_recovery.h"
#include "barr_cmd_version.h"
#include "barr_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BARR_RECOVERY_DIR ".barr_recovery"

barr_i32 BARR_command_recovery(barr_i32 argc, char **argv)
{
    if (!BARR_init())
    {
        return 1;
    }

    for (size_t i = 1; i < argc; ++i)
    {
        const char *opt = argv[i];

        if (BARR_strmatch(opt, "save"))
        {
            time_t now = time(NULL);
            if (now == ((time_t) -1))
            {
                return 1;
            }

            struct tm t;
            t = *localtime(&now);

            barr_i32 year = t.tm_year + 1900;
            barr_i32 month = t.tm_mon + 1;
            barr_i32 day = t.tm_mday;
            barr_i32 h = t.tm_hour;
            barr_i32 min = t.tm_min;
            barr_i32 sec = t.tm_sec;

            if (!BARR_isdir("build"))
            {
                BARR_errlog("%s(): build directory not found", __func__);
                return 1;
            }

            if (BARR_isdir_empty("build") || BARR_isdir_empty(".barr"))
            {
                BARR_errlog("%s(): build or .barr directory is empty", __func__);
                return 1;
            }

            BARR_mkdir_p(BARR_RECOVERY_DIR);

            char saved_dir[BARR_BUF_SIZE_1024];
            snprintf(saved_dir, sizeof(saved_dir), "%s/%d-%d-%d-%dh-%d%d", BARR_RECOVERY_DIR, year, month, day, h, min,
                     sec);

            BARR_mkdir_p(saved_dir);

            char cp_build_dir[BARR_PATH_MAX];
            snprintf(cp_build_dir, sizeof(cp_build_dir), "%s/build", saved_dir);

            char cp_barr_dir[BARR_PATH_MAX];
            snprintf(cp_barr_dir, sizeof(cp_barr_dir), "%s/.barr", saved_dir);

            // form the build and .barr dirr
            BARR_fs_copy_tree("build", cp_build_dir);
            BARR_printf("[RECOVERY]: saved build -> %s\n", cp_build_dir);
            BARR_fs_copy_tree(".barr", cp_barr_dir);
            BARR_printf("[RECOVERY]: saved .barr -> %s\n", cp_barr_dir);

            char cp_barrfile[BARR_PATH_MAX];
            snprintf(cp_barrfile, sizeof(cp_barrfile), "%s/Barrfile", saved_dir);
            BARR_printf("[RECOVERY]: saved Barrfile -> %s\n", cp_barrfile);

            BARR_file_copy("Barrfile", cp_barrfile);

            return 0;
        }

        if (BARR_strmatch(opt, "restore"))
        {
            if (!BARR_isdir(BARR_RECOVERY_DIR))
            {
                BARR_errlog("Recovery directory not found");
                return 1;
            }

            BARR_List recovery_list;
            BARR_list_init(&recovery_list, 16);
            BARR_scan_dir_shallow(&recovery_list, BARR_RECOVERY_DIR, BARR_SCAN_TYPE_DIR);

            if (recovery_list.count == 0)
            {
                BARR_errlog("No recovery states found");
                return 1;
            }

            BARR_rmrf("build");
            BARR_rmrf(".barr");

            for (size_t j = i; j < argc; ++j)
            {
                const char *target_path = NULL;

                if (BARR_isdigit_str(argv[j]))
                {
                    barr_i32 idx = atoi(argv[j]);
                    if (idx <= 0 || idx > recovery_list.count)
                    {
                        BARR_warnlog("Invalid recovery index: %s", argv[j]);
                        continue;
                    }
                    target_path = recovery_list.items[idx - 1];
                }
                else if (BARR_isdir(argv[j]))
                {
                    target_path = argv[j];
                }

                if (target_path)
                {
                    BARR_printf("This option overrides build and .barr directories, and Barrfile as well!\n");
                    BARR_printf("Are you sure you want to restore (%s)? [y/N]: ", target_path);
                    barr_i32 c = getchar();
                    if (c != 'y' && c != 'Y')
                    {
                        BARR_warnlog("Restore aborted!");
                        return 1;
                    }
                    while ((c = getchar()) != '\n' && c != EOF)
                    {
                    }

                    BARR_fs_copy_tree(target_path, BARR_getcwd());
                    BARR_log("Restoring: %s -> %s", target_path, BARR_getcwd());
                }
            }
            return 0;
        }

        if (BARR_strmatch(opt, "destroy"))
        {
            if (!BARR_isdir(BARR_RECOVERY_DIR))
            {
                BARR_errlog("Recovery directory not found");
                return 1;
            }

            BARR_printf("Are you sure you want to delete recovery files for (%s)? [y/N]: ", BARR_getcwd());
            barr_i32 c = getchar();
            if (c != 'y' && c != 'Y')
            {
                BARR_warnlog("Aborted!");
                return 1;
            }
            while ((c = getchar()) != '\n' && c != EOF)
            {
            }
            BARR_rmrf(BARR_RECOVERY_DIR);

            return 0;
        }

        if (BARR_strmatch(opt, "list"))
        {
            if (!BARR_isdir(BARR_RECOVERY_DIR))
            {
                BARR_errlog("Recovery directory not found");
                return 1;
            }

            BARR_List recovery_list;
            BARR_list_init(&recovery_list, 16);

            BARR_scan_dir_shallow(&recovery_list, BARR_RECOVERY_DIR, BARR_SCAN_TYPE_DIR);

            BARR_printf("Recovery List:\n");
            BARR_list_dbg(&recovery_list);

            return recovery_list.count > 0 ? 0 : 1;
        }
    }

    BARR_errlog("Recovery requires opt: <save|restore|destroy>");
    return 1;
}
