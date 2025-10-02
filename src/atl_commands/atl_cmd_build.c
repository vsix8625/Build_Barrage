#include "atl_cmd_build.h"
#include "atl_env.h"
#include "atl_glob_config_parser.h"
#include "atl_io.h"

atl_i32 ATL_command_build(atl_i32 argc, char **argv)
{
    // TODO: --path arg for remote build
    ATL_log("Hello from build command");

    ATL_ConfigTable *rc_table = ATL_config_parse_file(ATL_get_config("file"));

    ATL_ConfigEntry *atl_root_dir_entry = ATL_config_table_get(rc_table, "atl_root_dir");
    char *test = (atl_root_dir_entry && atl_root_dir_entry->value.str_val) ? atl_root_dir_entry->value.str_val : "NOPE";

    ATL_log("atl_root_dir_entry value=%s", test);

    rc_table->destroy(rc_table);
    ATL_VOID(argc);
    ATL_VOID(argv);
    return 0;
}
