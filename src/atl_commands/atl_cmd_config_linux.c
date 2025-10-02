#include "atl_cmd_config_linux.h"
#include "atl_env.h"
#include "atl_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *atl_get_shell_rc(char *buf, size_t buf_size)
{
    const char *shell = getenv("SHELL");
    if (!shell)
    {
        shell = "/bin/sh";
    }

    const char *shell_name = strrchr(shell, '/');
    shell_name = shell_name ? shell_name + 1 : shell;

    if (ATL_strmatch(shell_name, "bash"))
    {
        snprintf(buf, buf_size, "%s/.bashrc", ATL_GET_HOME());
    }
    else if (ATL_strmatch(shell_name, "zsh"))
    {
        snprintf(buf, buf_size, "%s/.zshrc", ATL_GET_HOME());
    }
    else if (ATL_strmatch(shell_name, "fish"))
    {
        snprintf(buf, buf_size, "%s/.config/fish/config.fish", ATL_GET_HOME());
    }
    else
    {
        snprintf(buf, buf_size, "%s/.profile", ATL_GET_HOME());  // fallback
    }

    return buf;
}

static void atl_append_path_line(const char *atl_bin_path)
{
    char rc_path[ATL_BUF_SIZE_512];
    atl_get_shell_rc(rc_path, sizeof(rc_path));

    FILE *check = fopen(rc_path, "r");
    if (check)
    {
        char line[ATL_BUF_SIZE_1024];
        while (fgets(line, sizeof(line), check))
        {
            if (strstr(line, ATL_CONFIG_PATH_MARKER))
            {
                fclose(check);
                return;  // already installed
            }
        }
        fclose(check);
    }

    FILE *f = fopen(rc_path, "a");
    if (!f)
    {
        ATL_errlog("Failed to open %s for appending", rc_path);
        return;
    }

    fprintf(f, "\n%s\nexport PATH=\"$PATH:%s\"\n", ATL_CONFIG_PATH_MARKER, atl_bin_path);
    fclose(f);

    ATL_log("Updated PATH in %s", rc_path);
}

static void atl_remove_path_line(void)
{
    char rc_path[ATL_BUF_SIZE_512];
    atl_get_shell_rc(rc_path, sizeof(rc_path));

    FILE *f = fopen(rc_path, "r");
    if (!f)
    {
        ATL_errlog("Failed to open %s for read", rc_path);
        return;
    }

    char tmp_path[ATL_BUF_SIZE_512];
    snprintf(tmp_path, sizeof(tmp_path), "%s/.tmp_atl_rc", ATL_GET_HOME());

    FILE *tmp = fopen(tmp_path, "w");
    if (!tmp)
    {
        fclose(f);
        ATL_errlog("Failed to open %s for read", tmp_path);
        return;
    }

    char line[ATL_BUF_SIZE_1024];
    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, ATL_CONFIG_PATH_MARKER))
        {
            fgets(line, sizeof(line), f);
            continue;
        }
        fputs(line, tmp);
    }

    fclose(f);
    fclose(tmp);
    rename(tmp_path, rc_path);
}

#define ATL_BIN_PATH "build/path_to_bin_dir.atl"

static const char *atl_get_bin_path(void)
{
    static char buf[ATL_BUF_SIZE_512];
    const char *tmp_path = ATL_BIN_PATH;
    FILE *f = fopen(tmp_path, "r");
    if (!f)
    {
        ATL_errlog("%s: Failed to open %s to read", __func__, tmp_path);
        return NULL;
    }

    if (fgets(buf, sizeof(buf), f))
    {
        buf[strcspn(buf, "\n")] = '\0';
        fclose(f);
        return buf;
    }
    fclose(f);
    return NULL;
}

//----------------------------------------------------------------------------------------------------

atl_i32 atl_config_install(atl_i32 argc, char **argv)
{
    if (atl_access("build/atl_install.lock", R_OK) != 0)
    {
        ATL_errlog("%s: atl config --install must run inside ATL root directory with an build/atl_install.lock",
                   __func__);
        return 1;
    }

    const char *atl_bin_path = atl_get_bin_path();
    if (!atl_bin_path)
    {
        ATL_errlog("Could not locate bin_fullpath.atl. Did you run ./scripts/build first");
        return 1;
    }

    atl_append_path_line(atl_bin_path);

    char config_dir[ATL_BUF_SIZE_512];
    snprintf(config_dir, sizeof(config_dir), "%s", ATL_get_config("dir"));
    if (!ATL_isdir(config_dir))
    {
        if (atl_mkdir(config_dir))
        {
            ATL_errlog("%s: Failed to create %s directory", __func__, config_dir);
            return 1;
        }
    }

    char config_file[ATL_BUF_SIZE_512];
    snprintf(config_file, sizeof(config_file), "%s", ATL_get_config("file"));

    if (!ATL_isfile(config_file))
    {
        FILE *f = fopen(config_file, "a");
        if (!f)
        {
            ATL_errlog("%s: Failed to create global config: %s", __func__, config_file);
            return 1;
        }
        fclose(f);
        ATL_log("Global config ensured: %s", config_file);
    }

    ATL_log("Atlas Build Manager (atl) installed successfully (user-local PATH updated)");
    ATL_log("Please run: source 'path/to/rc' or open a new shell and check $PATH for atl");
    ATL_VOID(argc);
    ATL_VOID(argv);
    return 0;
}

atl_i32 atl_config_uninstall(atl_i32 argc, char **argv)
{
    atl_remove_path_line();
    ATL_log("Atlas Build Manager uninstalled successfully (user-local PATH updated)");
    ATL_VOID(argc);
    ATL_VOID(argv);
    return 0;
}

atl_i32 ATL_cmd_config(atl_i32 argc, char **argv)
{
    if (argc < 2)
    {
        ATL_errlog("%s: config command requires option", __func__);
        return 1;
    }

    for (atl_i32 i = 1; i < argc; ++i)
    {
        if (ATL_strmatch(argv[i], "--install"))
        {
            return atl_config_install(argc, argv);
        }
        if (ATL_strmatch(argv[i], "--uninstall"))
        {
            return atl_config_uninstall(argc, argv);
        }
    }

    ATL_errlog("%s: Unknown option for config command", __func__);
    return 1;
}
