#include "barr_arena.h"
#include "barr_commands.h"
#include "barr_debug.h"
#include "barr_env.h"
#include "barr_gc.h"
#include "barr_io.h"
#include "barr_os_layer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static BARR_Arena    g_barr_cmds_arena;
static BARR_Command *g_barr_cmds_list  = NULL;
static size_t        g_barr_cmds_count = 0;

// from barr_env.h
bool g_barr_silent_logs = false;
bool g_barr_verbose     = false;
bool g_barr_trust       = false;

barr_i32 BARR_command_help(barr_i32 argc, char **argv);

static void barr_register_command(const char  *name,
                                  barr_cmd_fn  fn,
                                  const char  *help,
                                  const char **aliases,
                                  const char **options,
                                  const char  *detailed)
{
    if (!g_barr_cmds_list)
    {
        BARR_errlog("Command arena not initialized!");
        return;
    }

    if (g_barr_cmds_count > BARR_MAX_COMMANDS)
    {
        BARR_errlog("Max commands allowed reached: %d", BARR_MAX_COMMANDS);
        return;
    }

    BARR_Command *cmd = &g_barr_cmds_list[g_barr_cmds_count++];
    cmd->name         = name;
    cmd->fn           = fn;
    cmd->help         = help;
    cmd->aliases      = aliases;
    cmd->options      = options;
    cmd->detailed     = detailed;
}

//----------------------------------------------------------------------------------------------------
// Dispatch

static bool barr_is_chain_separator(const char *arg)
{
    return BARR_strmatch(arg, "...") || BARR_strmatch(arg, "---");
}

static barr_i32 barr_dispatch_single(barr_i32 argc, char **argv)
{
    const char *cmd = argv[0];

    for (size_t i = 0; i < g_barr_cmds_count; i++)
    {
        if (BARR_strmatch(cmd, g_barr_cmds_list[i].name))
        {
            return g_barr_cmds_list[i].fn(argc, argv);
        }

        if (g_barr_cmds_list[i].aliases)
        {
            for (const char **alias = g_barr_cmds_list[i].aliases; *alias; ++alias)
            {
                if (BARR_strmatch(cmd, *alias))
                {
                    return g_barr_cmds_list[i].fn(argc, argv);
                }
            }
        }
    }

    BARR_errlog("Unknown command: %s", cmd);
    return 1;
}

#define BARR_CLI_CHAIN_SEPARATOR_MARKER "::chain::"
#define BARR_CLI_MAX_CHAIN              32

static barr_i32 barr_dispatch_commands(barr_i32 argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (barr_is_chain_separator(argv[i]))
        {
            argv[i] = BARR_CLI_CHAIN_SEPARATOR_MARKER;  // internal marker
        }
    }

    barr_i32 ret       = 0;
    barr_i32 start     = 1;  // skip "barr"
    barr_i32 chain_idx = 0;

    for (barr_i32 i = 1; i <= argc; i++)
    {
        if (i == argc || BARR_strmatch(argv[i], BARR_CLI_CHAIN_SEPARATOR_MARKER))
        {
            barr_i32 sub_argc = i - start;
            if (sub_argc > 0)
            {
                if (chain_idx++ > BARR_CLI_MAX_CHAIN)
                {
                    BARR_errlog("Chain limit reached (%d commands max)", BARR_CLI_MAX_CHAIN);
                    return 1;
                }

                ret = barr_dispatch_single(sub_argc, &argv[start]);
                if (ret != 0)
                {
                    if (g_barr_verbose)
                    {
                        BARR_errlog("Stopped at command (%d:%s)", chain_idx, argv[start]);
                    }
                    return ret;  // stop chain if any command fails
                }
            }
            else
            {
                if (g_barr_verbose)
                {
                    BARR_warnlog("Skipped empty segment between separators: %d-(%s)", i, argv[i]);
                }
            }
            start = i + 1;  // skip chain  marker
        }
    }

    return ret;
}

//----------------------------------------------------------------------------------------------------
// MAIN
barr_i32 main(barr_i32 argc, char **argv)
{
    if (argc < 2)
    {
        BARR_errlog("Usage: barr <command> | barr help");
        return 1;
    }

    bool     do_gc_dump = false;
    barr_i32 new_argc   = 1;

    for (barr_i32 i = 1; i < argc; i++)
    {
        if (BARR_strmatch(argv[i], "--gc-dump"))
        {
            do_gc_dump = true;
        }
        else if (BARR_strmatch(argv[i], "--silent"))
        {
            g_barr_silent_logs = true;
        }
        else if (BARR_strmatch(argv[i], "--verbose"))
        {
            g_barr_verbose = true;
        }
        else if (BARR_strmatch(argv[i], "--trust"))
        {
            if (g_barr_silent_logs)
            {
                BARR_log("Barrfile run_cmd() will not ask for approval");
            }
            g_barr_trust = true;
        }
        else
        {
            if (new_argc != i)
            {
                argv[new_argc] = argv[i];
            }
            new_argc++;
        }
    }
    argc = new_argc;

    if (g_barr_silent_logs)
    {
        FILE *null_out = fopen(BARR_DEVNULL, "w");
        if (null_out)
        {
            fflush(stdout);
            fflush(stderr);
            dup2(fileno(null_out), STDOUT_FILENO);
            dup2(fileno(null_out), STDERR_FILENO);
            fclose(null_out);
        }
    }

    // init commands arena
    BARR_arena_init(
        &g_barr_cmds_arena, sizeof(BARR_Command) * BARR_MAX_COMMANDS, "commands_arena", 8);
    g_barr_cmds_list  = (BARR_Command *) BARR_arena_alloc(&g_barr_cmds_arena,
                                                          sizeof(BARR_Command) * BARR_MAX_COMMANDS);
    g_barr_cmds_count = 0;

    // init GC
    BARR_gc_init();

    //----------------------------------------------------------------------------------------------------
    // Register commands

    // help
    static const char *help_aliases[] = {"--help", "-h", NULL};  // null terminated list
    barr_register_command("help",
                          BARR_command_help,
                          "Show general help or help for specific command",
                          help_aliases,
                          NULL,
                          NULL);

    // version
    static const char *ver_aliases[] = {"--version", "-v", NULL};
    barr_register_command(
        "version", BARR_command_version, "Show version information", ver_aliases, NULL, NULL);

    // build
    static const char *build_aliases[] = {"-b", NULL};
    static const char *build_opts[]    = {
        "--dir <dirpath>", "--dry-run", "--turbo", "--threads [N]| -j[N]", NULL};
    barr_register_command(
        "build", BARR_command_build, "Build project", build_aliases, build_opts, NULL);

    // rebuild
    static const char *rebuild_aliases[] = {"-rb", NULL};
    barr_register_command(
        "rebuild", BARR_command_rebuild, "Clean and build project", rebuild_aliases, NULL, NULL);

    // run
    static const char *run_aliases[] = {"-r", NULL};
    barr_register_command("run", BARR_command_run, "Run latest binary", run_aliases, NULL, NULL);

    // tool
    static const char *tool_aliases[] = {"-tl", NULL};
    static const char *tool_opts[]    = {"--gdb", "--perf", "--valgrind", "--strace", NULL};
    barr_register_command("tool", BARR_command_tool, "Run tool", tool_aliases, tool_opts, NULL);

    // clean
    static const char *clean_aliases[] = {"-c", NULL};
    static const char *clean_opts[]    = {"--no-confirm | -nc", NULL};
    barr_register_command(
        "clean", BARR_command_clean, "Clean build directory", clean_aliases, clean_opts, NULL);

    // init
    static const char *init_aliases[] = {"-I", NULL};
    barr_register_command("init",
                          BARR_command_init,
                          "Initialize Build Barrage in current working directory",
                          init_aliases,
                          NULL,
                          NULL);

    // deinit
    static const char *deinit_aliases[] = {"-D", NULL};
    barr_register_command("deinit",
                          BARR_command_deinit,
                          "Uninitialize Build Barrage in current working directory",
                          deinit_aliases,
                          NULL,
                          NULL);

    // new
    static const char *new_details =
        " --project <name>      # Creates <name> directory with Build Barrage "
        "initialized (can run "
        "anywhere)\n\n"
        "  --file <name> [dirpath]       # Creates <name> file with optional directory\n"
        "  --barrfile                    # Creates a default Barrfile\n"
        "  --main                        # Creates src/main.c with Hello,from barr\n"
        "  --pair <file_name> [--dir] <dir_name> [--ext] <ext>\n\n"
        "Examples: barr new --project my_project\n"
        "          barr new --file main.c\n"
        "          barr new --file main.c src\n"
        "          barr new --pair core --dir engine                   # Creates: "
        "engine/core.c, engine/core.h\n"
        "          barr new --pair graphics --ext .cpp --public true   # Creates: "
        "src/graphics.cpp, "
        "inc/graphics.hpp\n"
        "Note: --pair creates the directory if does not exists\n"
        "\t If --dir not specified it defaults to src directory\n"
        "\t If --public true is set, creates an include directory for header\n";
    static const char *new_aliases[] = {"-n", NULL};
    static const char *new_opts[] = {"--project", "--file", "--pair", "--barrfile", "--main", NULL};
    barr_register_command(
        "new", BARR_command_new, "Create new project or files", new_aliases, new_opts, new_details);

    // config
    static const char *config_details = "   open        Open local Barrfile with your $EDITOR "
                                        "(requires barr initialized project)\n";

    static const char *config_aliases[] = {"-C", NULL};
    static const char *config_opts[]    = {"open | -O", NULL};
    barr_register_command("config",
                          BARR_cmd_config,
                          "Manage configuration",
                          config_aliases,
                          config_opts,
                          config_details);

    // status
    static const char *status_details = "\t--vars         # Print all project Barrfile variables\n"
                                        "\t--binfo        # Print project build info\n"
                                        "\t--all          # Print all project source files\n";
    static const char *status_aliases[] = {"-s", NULL};
    static const char *status_opts[]    = {"--vars", "--all", "--binfo", NULL};
    barr_register_command("status",
                          BARR_cmd_status,
                          "Project status information",
                          status_aliases,
                          status_opts,
                          status_details);

    // fo
    static const char *fo_alias[] = {NULL};
    static const char *fo_opts[]  = {"deploy | -d", "dismiss | -s", "report", "watch | -w", NULL};
    barr_register_command(
        "fo", BARR_command_fo, "Project forward observer", fo_alias, fo_opts, NULL);

    // debug
    static const char *debug_details   = "\t--fsinfo       # Show project dirs and files\n"
                                         "\t--cache        # Print project cached files\n";
    static const char *debug_aliases[] = {"-db", NULL};
    static const char *debug_opts[]    = {"--fsinfo", "--cache", NULL};
    barr_register_command(
        "debug", BARR_cmd_debug, "Debug and information", debug_aliases, debug_opts, debug_details);

    static const char *install_aliases[] = {"-i", NULL};
    static const char *install_opts[]    = {"--destdir", "--prefix", NULL};
    barr_register_command(
        "install", BARR_command_install, "System install", install_aliases, install_opts, NULL);

    static const char *uninstall_aliases[] = {"-uni", NULL};
    barr_register_command(
        "uninstall", BARR_command_uninstall, "System uninstall", uninstall_aliases, NULL, NULL);

    //----------------------------------------------------------------------------------------------------
    // Dispatch

    barr_i32 dispatch_ret = barr_dispatch_commands(argc, argv);

    // -------------------------------------------------------
    // Cleanup perf
    struct timespec gc_cleanup_start, gc_cleanup_end;
    clock_gettime(CLOCK_MONOTONIC, &gc_cleanup_start);

    if (do_gc_dump)
    {
        BARR_gc_file_dump();
    }
    BARR_gc_shutdown();

    clock_gettime(CLOCK_MONOTONIC, &gc_cleanup_end);
    BARR_dbglog("BARR_gc cleanup time:\033[34;1m %s",
                BARR_fmt_time_elapsed(&gc_cleanup_start, &gc_cleanup_end));

    // -------------------------------------------------------

    BARR_destroy_arena(&g_barr_cmds_arena);

    //----------------------------------------------------------------------------------------------------
    return dispatch_ret;
}

//----------------------------------------------------------------------------------------------------

#define CYAN   "\x1b[36m"
#define YELLOW "\x1b[33m"
#define RESET  "\x1b[0m"
#define BOLD   "\x1b[1m"

static inline barr_i32 barr_istty(void)
{
    return isatty(fileno(stdout));
}

barr_i32 BARR_command_help(barr_i32 argc, char **argv)
{
    if (system(BARR_SYSTEM_CMD_CLEAR))
    {
        BARR_errlog("System command %s failed", BARR_SYSTEM_CMD_CLEAR);
    }

    // individual command help
    if (argc == 2)
    {
        const char *cmd = argv[1];
        for (size_t i = 0; i < g_barr_cmds_count; i++)
        {
            if (BARR_strmatch(g_barr_cmds_list[i].name, cmd))
            {
                BARR_printf("Command: %s\n", cmd);
                BARR_printf("Description: %s\n", g_barr_cmds_list[i].help);
                if (g_barr_cmds_list[i].aliases)
                {
                    BARR_printf("Aliases: ");
                    for (const char **alias = g_barr_cmds_list[i].aliases; *alias; ++alias)
                    {
                        BARR_printf("%s ", *alias);
                    }

                    if (g_barr_cmds_list[i].options)
                    {
                        BARR_printf("\n\nOptions:\n");
                        for (const char **opt = g_barr_cmds_list[i].options; *opt; ++opt)
                        {
                            BARR_printf("  %s\n", *opt);
                        }
                    }

                    BARR_printf("\n");
                    if (g_barr_cmds_list[i].detailed)
                    {
                        BARR_printf("Details:\n%s\n", g_barr_cmds_list[i].detailed);
                    }
                }
                return 0;
            }

            if (g_barr_cmds_list[i].aliases)
            {
                for (const char **alias = g_barr_cmds_list[i].aliases; *alias; ++alias)
                {
                    if (BARR_strmatch(*alias, cmd))
                    {
                        BARR_printf("Command: %s (alias: %s)\n", g_barr_cmds_list[i].name, *alias);
                        BARR_printf("Description: %s\n", g_barr_cmds_list[i].help);

                        if (g_barr_cmds_list[i].options)
                        {
                            BARR_printf("\n\nOptions:\n");
                            for (const char **opt = g_barr_cmds_list[i].options; *opt; ++opt)
                            {
                                BARR_printf("  %s\n", *opt);
                            }
                        }

                        if (g_barr_cmds_list[i].detailed)
                        {
                            BARR_printf("\nDetails:\n %s\n", g_barr_cmds_list[i].detailed);
                        }

                        return 0;
                    }
                }
            }
        }

        BARR_errlog("Unknown command: %s", cmd);
        return 1;
    }

    // general help output
    BARR_printf("--------------------------------------------------\n");
    BARR_command_version(argc, argv);
    BARR_printf("--------------------------------------------------\n");
    BARR_printf("Usage: barr <command> [options]\n\n");
    BARR_printf("List of commands:\n\n");

    size_t max_name_len  = 0;
    size_t max_alias_len = 0;

    // compute max widths
    for (size_t i = 0; i < g_barr_cmds_count; ++i)
    {
        size_t name_len = strlen(g_barr_cmds_list[i].name);
        if (name_len > max_name_len)
        {
            max_name_len = name_len;
        }

        size_t alias_len = 0;
        if (g_barr_cmds_list[i].aliases)
        {
            for (const char **alias = g_barr_cmds_list[i].aliases; *alias; ++alias)
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
    barr_i32 name_col_width  = (barr_i32) max_name_len + 4;
    barr_i32 alias_col_width = (barr_i32) max_alias_len + 4;

    if (barr_istty())
    {
        BARR_printf(BOLD CYAN "  %-*s%-*s%s\n" RESET,
                    name_col_width,
                    "COMMAND",
                    alias_col_width,
                    "ALIASES",
                    "DESCRIPTION");
    }
    else
    {
        BARR_printf(
            "  %-*s%-*s%s\n", name_col_width, "COMMAND", alias_col_width, "ALIASES", "DESCRIPTION");
    }

    BARR_printf("  %-*.*s%-*.*s%s\n",
                name_col_width,
                name_col_width,
                "----------------------------------------",
                alias_col_width,
                alias_col_width,
                "----------------------------------------",
                "----------------------------------------");

    for (size_t i = 0; i < g_barr_cmds_count; ++i)
    {
        char alias_buf[128] = {0};
        if (g_barr_cmds_list[i].aliases)
        {
            for (const char **alias = g_barr_cmds_list[i].aliases; *alias; ++alias)
            {
                strcat(alias_buf, *alias);
                strcat(alias_buf, " ");
            }
        }

        if (barr_istty())
        {
            BARR_printf("  " YELLOW "%-*s" RESET "%-*s%s\n",
                        name_col_width,
                        g_barr_cmds_list[i].name,
                        alias_col_width,
                        alias_buf,
                        g_barr_cmds_list[i].help);
        }
        else
        {
            BARR_printf("  %-*s%-*s%s\n",
                        name_col_width,
                        g_barr_cmds_list[i].name,
                        alias_col_width,
                        alias_buf,
                        g_barr_cmds_list[i].help);
        }
    }

    BARR_printf("\nGlobal commands:\n"
                "\t--verbose: More verbose logs \n"
                "\t--silent: Mute logs\n"
                "\t--trust: Executes run_cmd() without prompting\n"
                "\t--gc-dump: Dump all memory allocations on process call\n"
                "\n\n");
    BARR_printf("For more details run: %sbarr %shelp%s <command_name>\n", CYAN, YELLOW, RESET);
    return 0;
}
