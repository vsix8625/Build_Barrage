// Created by Build Barrage
#include "barr_cmd_debug.h"
#include "barr_cmd_version.h"
#include "barr_io.h"
#include "barr_list.h"

barr_i32 BARR_cmd_debug(barr_i32 argc, char **argv)
{
    if (!BARR_init())
    {
        return 1;
    }

    for (barr_i32 i = 0; i < argc; ++i)
    {
        const char *opt = argv[i];

        if (BARR_strmatch(opt, "--fsinfo"))
        {
            BARR_List info_list;
            BARR_list_init(&info_list, 64);

            BARR_fsinfo_collect_stats_dir_r(&info_list, BARR_getcwd());
            BARR_list_fsinfo_print(&info_list);
        }

        if (BARR_strmatch(opt, "--cache"))
        {
            if (!BARR_isfile(BARR_CACHE_FILE))
            {
                BARR_errlog("No cache file found");
                return 1;
            }

            BARR_printf("%s", BARR_file_read(BARR_CACHE_FILE));
        }
    }
    return 0;
}
