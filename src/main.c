#include "atl_arena.h"
#include "atl_commands.h"
#include "atl_env.h"
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

//----------------------------------------------------------------------------------------------------
// Dispatch

static bool atl_is_chain_separator(const char *arg)
{
    return ATL_strmatch(arg, "...") || ATL_strmatch(arg, "---");
}

static atl_i32 atl_dispatch_single(atl_i32 argc, char **argv)
{
    const char *cmd = argv[0];

    for (size_t i = 0; i < g_atl_cmds_count; i++)
    {
        if (ATL_strmatch(cmd, g_atl_cmds_list[i].name))
        {
            return g_atl_cmds_list[i].fn(argc, argv);
        }

        if (g_atl_cmds_list[i].aliases)
        {
            for (const char **alias = g_atl_cmds_list[i].aliases; *alias; ++alias)
            {
                if (ATL_strmatch(cmd, *alias))
                {
                    return g_atl_cmds_list[i].fn(argc, argv);
                }
            }
        }
    }

    ATL_errlog("Unknown command: %s", cmd);
    return 1;
}

#define ATL_CLI_CHAIN_SEPARATOR_MARKER "::chain::"
#define ATL_CLI_MAX_CHAIN 32

static atl_i32 atl_dispatch_commands(atl_i32 argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (atl_is_chain_separator(argv[i]))
        {
            argv[i] = ATL_CLI_CHAIN_SEPARATOR_MARKER;  // internal marker
        }
    }

    atl_i32 ret = 0;
    atl_i32 start = 1;  // skip "atl"
    atl_i32 chain_idx = 0;

    for (atl_i32 i = 1; i <= argc; i++)
    {
        if (i == argc || ATL_strmatch(argv[i], ATL_CLI_CHAIN_SEPARATOR_MARKER))
        {
            atl_i32 sub_argc = i - start;
            if (sub_argc > 0)
            {
                if (chain_idx++ > ATL_CLI_MAX_CHAIN)
                {
                    ATL_errlog("Chain limit reached (%d commands max)", ATL_CLI_MAX_CHAIN);
                    return 1;
                }

                ATL_dbglog("Executing command (%d:%s)", chain_idx, argv[start]);
                ret = atl_dispatch_single(sub_argc, &argv[start]);
                if (ret != 0)
                {
                    ATL_errlog("Stopped at command (%d:%s)", chain_idx, argv[start]);
                    return ret;  // stop chain if any command fails
                }
            }
            else
            {
                ATL_warnlog("Skipped empty segment between separators: %d-(%s)", i, argv[i]);
            }
            start = i + 1;  // skip chain  marker
        }
    }

    return ret;
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

    // build
    static const char *build_aliases[] = {"-b", NULL};
    atl_register_command("build", ATL_command_build, "Build project", build_aliases);

    // version
    static const char *ver_aliases[] = {"--version", "-v", NULL};
    atl_register_command("version", ATL_command_version, "Print version information", ver_aliases);

    // new
    static const char *new_aliases[] = {"-n", NULL};
    atl_register_command("new", ATL_command_new, "Print version information", new_aliases);

    // config
    static const char *config_aliases[] = {"-C", NULL};
    atl_register_command("config", ATL_cmd_config, "Install, uninstall and edit configuration", config_aliases);

    //----------------------------------------------------------------------------------------------------
    // Dispatch

    atl_i32 dispatch_ret = atl_dispatch_commands(argc, argv);

    //----------------------------------------------------------------------------------------------------

    // Cleanup
    ATL_destroy_arena(&g_atl_cmds_arena);

    //----------------------------------------------------------------------------------------------------
    return dispatch_ret;
}
