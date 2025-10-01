#include "atl_arena.h"
#include "atl_commands.h"
#include "atl_io.h"

static ATL_Arena g_atl_cmds_arena;
static ATL_Command *g_atl_cmds_list = NULL;
static size_t g_atl_cmds_count = 0;

static void atl_register_command(const char *name, atl_cmd_fn fn, const char *help, const char **aliases)
{
    if (!g_atl_cmds_list)
    {
        ATL_errlog("Command arena not initialized!");
        return;
    }

    if (g_atl_cmds_count > ATL_MAX_COMMANDS)
    {
        ATL_errlog("Max commands allowed reached: %d", ATL_MAX_COMMANDS);
        return;
    }

    ATL_Command *cmd = &g_atl_cmds_list[g_atl_cmds_count++];
    cmd->name = name;
    cmd->fn = fn;
    cmd->help = help;
    cmd->aliases = aliases;
}

static atl_i32 atl_dispatch_commands(atl_i32 argc, char **argv)
{
    const char *cmd = argv[1];

    for (size_t i = 0; i < g_atl_cmds_count; i++)
    {
        if (ATL_strmatch(cmd, g_atl_cmds_list[i].name))
            return g_atl_cmds_list[i].fn(argc - 1, &argv[1]);

        if (g_atl_cmds_list[i].aliases)
        {
            for (const char **alias = g_atl_cmds_list[i].aliases; *alias; ++alias)
            {
                if (ATL_strmatch(cmd, *alias))
                    return g_atl_cmds_list[i].fn(argc - 1, &argv[1]);
            }
        }
    }

    ATL_errlog("Unknown command: %s", cmd);
    return 1;
}

atl_i32 main(atl_i32 argc, char **argv)
{
    if (argc < 2)
    {
        ATL_errlog("Usage: atl <command>");
        return 1;
    }

    // init commands arena
    ATL_arena_init(&g_atl_cmds_arena, sizeof(ATL_Command) * ATL_MAX_COMMANDS);
    g_atl_cmds_list = (ATL_Command *) ATL_arena_alloc(&g_atl_cmds_arena, sizeof(ATL_Command) * ATL_MAX_COMMANDS);
    g_atl_cmds_count = 0;

    //----------------------------------------------------------------------------------------------------
    // Register commands

    // help command
    static const char *help_aliases[] = {"--help", "-h", NULL};  // null terminated list
    atl_register_command("help", ATL_command_help, "Print help information", help_aliases);

    // version
    static const char *ver_aliases[] = {"--version", "-v", NULL};  // null terminated list
    atl_register_command("version", ATL_command_version, "Print version information", ver_aliases);

    //----------------------------------------------------------------------------------------------------
    // Dispatch

    atl_i32 dispatch_ret = atl_dispatch_commands(argc, argv);

    //----------------------------------------------------------------------------------------------------

    // Cleanup
    ATL_destroy_arena(&g_atl_cmds_arena);

    //----------------------------------------------------------------------------------------------------
    return dispatch_ret;
}
