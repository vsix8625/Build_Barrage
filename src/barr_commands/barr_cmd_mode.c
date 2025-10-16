#include "barr_cmd_mode.h"
#include "barr_env.h"
#include "barr_glob_config_keys.h"
#include "barr_glob_config_parser.h"
#include "barr_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool barr_is_mode_still_active(const char *expected)
{
    char *home = BARR_GET_HOME();
    char fullpath[BARR_PATH_MAX];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", home, BARR_LOCAL_SHARE_MODES_ACTIVE_FILE);

    FILE *f = fopen(fullpath, "r");
    if (!f)
    {
        return false;
    }

    char buf[BARR_BUF_SIZE_64];
    if (!fgets(buf, sizeof(buf), f))
    {
        fclose(f);
        return false;
    }
    fclose(f);

    buf[strcspn(buf, "\n")] = '\0';
    return BARR_strmatch(buf, expected);
}

barr_i32 BARR_command_mode(barr_i32 argc, char **argv)
{
    const char *conf_file = BARR_get_config("file");
    BARR_ConfigTable *conf_table = BARR_config_parse_file(conf_file);

    BARR_ConfigEntry *root_dir_entry = BARR_config_table_get(conf_table, BARR_GLOB_CONFIG_KEY_ROOT_DIR);
    if (!root_dir_entry)
    {
        conf_table->destroy(conf_table);
        return 1;
    }
    char *root_dir_str = root_dir_entry->value.str_val;

    BARR_ConfigEntry *data_dir_entry = BARR_config_table_get(conf_table, BARR_GLOB_CONFIG_KEY_DATA_DIR);
    if (!data_dir_entry)
    {
        conf_table->destroy(conf_table);
        return 1;
    }
    char *data_dir_str = data_dir_entry->value.str_val;

    for (barr_i32 i = 1; i < argc; ++i)
    {
        if (BARR_strmatch(argv[i], "OFF"))
        {
            if (barr_is_mode_still_active("off"))
            {
                BARR_warnlog("All modes are already turned OFF");
                break;
            }
            char write_active_file_path[BARR_PATH_MAX];
            snprintf(write_active_file_path, sizeof(write_active_file_path), "%s/modes/active", data_dir_str);
            BARR_file_write(write_active_file_path, "off");

            BARR_log("All modes turned OFF");
            break;
        }
        else if (BARR_strmatch(argv[i], "STATUS"))
        {
            char active_file_path[BARR_PATH_MAX];
            snprintf(active_file_path, sizeof(active_file_path), "%s/modes/active", data_dir_str);
            if (BARR_isfile(active_file_path))
            {
                FILE *f = fopen(active_file_path, "r");
                if (!f)
                {
                    return false;
                }

                char buf[BARR_BUF_SIZE_64];
                if (!fgets(buf, sizeof(buf), f))
                {
                    fclose(f);
                    return false;
                }
                fclose(f);

                buf[strcspn(buf, "\n")] = '\0';

                BARR_log("[Active-mode]: %s", buf);
                break;
            }
        }
        else if (BARR_strmatch(argv[i], "WAR"))
        {
            char exec_path[BARR_PATH_MAX];
            snprintf(exec_path, sizeof(exec_path), "%s/modes/war/war_mode", root_dir_str);

            char write_active_file_path[BARR_PATH_MAX];
            snprintf(write_active_file_path, sizeof(write_active_file_path), "%s/modes/active", data_dir_str);
            BARR_file_write(write_active_file_path, "war");

            char *war_mode[] = {exec_path, NULL};
            barr_i32 bg_pid = BARR_run_process_BG(war_mode[0], war_mode, true);
            if (bg_pid > 0)
            {
                BARR_log("WAR mode running in background with PID %d", bg_pid);
            }
            break;
        }
        else
        {
            BARR_errlog("Invalid \"mode\" option: %s", argv[i]);
            return 1;
        }
    }

    conf_table->destroy(conf_table);
    return 0;
}
