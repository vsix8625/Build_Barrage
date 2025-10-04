#include "atl_arena.h"
#include "atl_commands.h"
#include "atl_debug.h"
#include "atl_env.h"
#include "atl_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ATL_Arena g_atl_cmds_arena;
static ATL_Command *g_atl_cmds_list = NULL;
static size_t g_atl_cmds_count = 0;

atl_i32 ATL_command_help(atl_i32 argc, char **argv);

static void atl_register_command(const char *name, atl_cmd_fn fn, const char *help, const char **aliases,
                                 const char *detailed)
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
    cmd->detailed = detailed;
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

//----------------------------------------------------------------------------------------------------
// MAIN
atl_i32 main(atl_i32 argc, char **argv)
{
    if (argc < 2)
    {
        ATL_errlog("Usage: atl <command>");
        return 1;
    }

    // init commands arena
    ATL_arena_init(&g_atl_cmds_arena, sizeof(ATL_Command) * ATL_MAX_COMMANDS, "commands_arena");
    g_atl_cmds_list = (ATL_Command *) ATL_arena_alloc(&g_atl_cmds_arena, sizeof(ATL_Command) * ATL_MAX_COMMANDS);
    g_atl_cmds_count = 0;

    //----------------------------------------------------------------------------------------------------
    // Register commands

    // help
    static const char *help_aliases[] = {"--help", "-h", NULL};  // null terminated list
    atl_register_command("help", ATL_command_help, "Show general help or help for specific command", help_aliases,
                         NULL);

    // version
    static const char *ver_aliases[] = {"--version", "-v", NULL};
    atl_register_command("version", ATL_command_version, "Show version information", ver_aliases, NULL);

    // build
    static const char *build_aliases[] = {"-b", NULL};
    atl_register_command("build", ATL_command_build, "Build project", build_aliases, NULL);

    // init
    static const char *init_aliases[] = {"-I", NULL};
    atl_register_command("init", ATL_command_init, "Initialize atl in current working directory", init_aliases, NULL);

    // deinit
    static const char *deinit_aliases[] = {"-D", NULL};
    atl_register_command("deinit", ATL_command_deinit, "Uninitialize atl in current working directory", deinit_aliases,
                         NULL);

    // new
    static const char *new_details =
        "Options:\n"
        "  --project <project_name>          Create <project_name> directory with atl initialized (can run anywhere)\n"
        "  --file <file_name> <opt=dir_name> Create <file_name> file with optional directory (requires "
        "initialized atl project)\n"
        "Example: atl new --project my_project\n"
        "         atl new --file main.c\n"
        "         atl new --file main.c src\n";
    static const char *new_aliases[] = {"-n", NULL};
    atl_register_command("new", ATL_command_new, "Create new project or files", new_aliases, new_details);

    // config
    static const char *config_details =
        "Options:\n"
        "   --install   Add atl binary directory path to $PATH and a default global config, ideal for dev stage\n"
        "   --uninstall Remove atl binary directoy from $PATH and removes global config directory\n"
        "   --view      Show current global config (can run anywhere)\n"
        "   --edit      Open global config with your $EDITOR (can run anywhere)\n"
        "\nNote: --install and --uninstall can only be run from the Atlas Build Manager root directory\n";
    static const char *config_aliases[] = {"-C", NULL};
    atl_register_command("config", ATL_cmd_config, "Manage configuration", config_aliases, config_details);

    // status
    static const char *status_aliases[] = {"-S", NULL};
    atl_register_command("status", ATL_cmd_status, "View status information", status_aliases, NULL);

    //----------------------------------------------------------------------------------------------------
    // Dispatch

    atl_i32 dispatch_ret = atl_dispatch_commands(argc, argv);

    //----------------------------------------------------------------------------------------------------

    // Cleanup
    ATL_destroy_arena(&g_atl_cmds_arena);

    //----------------------------------------------------------------------------------------------------
    return dispatch_ret;
}

//----------------------------------------------------------------------------------------------------

#define CYAN "\x1b[36m"
#define YELLOW "\x1b[33m"
#define RESET "\x1b[0m"
#define BOLD "\x1b[1m"

static inline atl_i32 atl_istty(void)
{
    return isatty(fileno(stdout));
}

atl_i32 ATL_command_help(atl_i32 argc, char **argv)
{
    if (system(ATL_SYSTEM_CMD_CLEAR))
    {
        ATL_errlog("System command %s failed", ATL_SYSTEM_CMD_CLEAR);
    }

    // individual command help
    if (argc == 2)
    {
        const char *cmd = argv[1];
        for (size_t i = 0; i < g_atl_cmds_count; i++)
        {
            if (ATL_strmatch(g_atl_cmds_list[i].name, cmd))
            {
                ATL_printf("Command: %s\n", cmd);
                ATL_printf("Description: %s\n", g_atl_cmds_list[i].help);
                if (g_atl_cmds_list[i].aliases)
                {
                    ATL_printf("Aliases: ");
                    for (const char **alias = g_atl_cmds_list[i].aliases; *alias; ++alias)
                    {
                        ATL_printf("%s ", *alias);
                    }
                    ATL_printf("\n");
                    if (g_atl_cmds_list[i].detailed)
                    {
                        ATL_printf("Details:\n%s\n", g_atl_cmds_list[i].detailed);
                    }
                }
                return 0;
            }

            if (g_atl_cmds_list[i].aliases)
            {
                for (const char **alias = g_atl_cmds_list[i].aliases; *alias; ++alias)
                {
                    if (ATL_strmatch(*alias, cmd))
                    {
                        ATL_printf("Command: %s (alias: %s)\n", g_atl_cmds_list[i].name, *alias);
                        ATL_printf("Description: %s\n", g_atl_cmds_list[i].help);

                        if (g_atl_cmds_list[i].detailed)
                        {
                            ATL_printf("\nDetails:\n %s\n", g_atl_cmds_list[i].detailed);
                        }

                        return 0;
                    }
                }
            }
        }

        ATL_errlog("Unknown command: %s", cmd);
        return 1;
    }

    // general help output
    ATL_printf("--------------------------------------------------\n");
    ATL_command_version(argc, argv);
    ATL_printf("--------------------------------------------------\n");
    ATL_printf("Usage: atl <command> [options]\n\n");
    ATL_printf("List of commands:\n\n");

    size_t max_name_len = 0;
    size_t max_alias_len = 0;

    // compute max widths
    for (size_t i = 0; i < g_atl_cmds_count; ++i)
    {
        size_t name_len = strlen(g_atl_cmds_list[i].name);
        if (name_len > max_name_len)
        {
            max_name_len = name_len;
        }

        size_t alias_len = 0;
        if (g_atl_cmds_list[i].aliases)
        {
            for (const char **alias = g_atl_cmds_list[i].aliases; *alias; ++alias)
            {
                alias_len += strlen(*alias) + 1;
            }
        }
        if (alias_len > max_alias_len)
        {
            max_alias_len = alias_len;
        }
    }

    // fixed extra padding
    atl_i32 name_col_width = (atl_i32) max_name_len + 4;
    atl_i32 alias_col_width = (atl_i32) max_alias_len + 4;

    if (atl_istty())
    {
        ATL_printf(BOLD CYAN "  %-*s%-*s%s\n" RESET, name_col_width, "COMMAND", alias_col_width, "ALIASES",
                   "DESCRIPTION");
    }
    else
    {
        ATL_printf("  %-*s%-*s%s\n", name_col_width, "COMMAND", alias_col_width, "ALIASES", "DESCRIPTION");
    }

    ATL_printf("  %-*.*s%-*.*s%s\n", name_col_width, name_col_width, "----------------------------------------",
               alias_col_width, alias_col_width, "----------------------------------------",
               "----------------------------------------");

    for (size_t i = 0; i < g_atl_cmds_count; ++i)
    {
        char alias_buf[128] = {0};
        if (g_atl_cmds_list[i].aliases)
        {
            for (const char **alias = g_atl_cmds_list[i].aliases; *alias; ++alias)
            {
                strcat(alias_buf, *alias);
                strcat(alias_buf, " ");
            }
        }

        if (atl_istty())
        {
            ATL_printf("  " YELLOW "%-*s" RESET "%-*s%s\n", name_col_width, g_atl_cmds_list[i].name, alias_col_width,
                       alias_buf, g_atl_cmds_list[i].help);
        }
        else
        {
            ATL_printf("  %-*s%-*s%s\n", name_col_width, g_atl_cmds_list[i].name, alias_col_width, alias_buf,
                       g_atl_cmds_list[i].help);
        }
    }

    ATL_printf("\n");
    ATL_printf("For more details run: %satl %shelp%s <command_name>\n", CYAN, YELLOW, RESET);
    return 0;
}
