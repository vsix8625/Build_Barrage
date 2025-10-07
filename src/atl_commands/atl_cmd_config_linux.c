#include "atl_cmd_config_linux.h"
#include "atl_debug.h"
#include "atl_env.h"
#include "atl_glob_config_keys.h"
#include "atl_glob_config_parser.h"
#include "atl_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *atl_get_bin_path(void);

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

#define ATL_RC_BLOCK_BEGIN "# >>> Atlas Build Manager >>>"
#define ATL_RC_BLOCK_END "# <<< Atlas Build Manager <<<"

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
            if (strstr(line, ATL_RC_BLOCK_BEGIN))
            {
                fclose(check);
                ATL_log("Atlas block already present in %s", rc_path);
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

    fprintf(f,
            "\n%s\ncase \":$PATH:\" in\n"
            " *\":%s:\"*) ;;\n"
            " *) PATH=\"$PATH:%s\" ;;\n"
            "esac\n"
            "export PATH\n"
            "%s\n",
            ATL_RC_BLOCK_BEGIN, atl_bin_path, atl_bin_path, ATL_RC_BLOCK_END);
    fclose(f);

    ATL_log("Added atl PATH block in %s", rc_path);
}

static void atl_remove_path_line(void)
{
    char rc_path[ATL_BUF_SIZE_512];
    atl_get_shell_rc(rc_path, sizeof(rc_path));

    // make a backup
    char backup_path[ATL_BUF_SIZE_1024];
    snprintf(backup_path, sizeof(backup_path), "%s.bak", rc_path);
    if (ATL_file_copy(rc_path, backup_path) == 0)
    {
        ATL_log("Backup created: %s", backup_path);
    }
    else
    {
        ATL_warnlog("Could not create backup of %s", rc_path);
    }

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
    bool in_block = false;

    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, ATL_RC_BLOCK_BEGIN))
        {
            in_block = true;
            continue;  // skip begin marker
        }

        if (strstr(line, ATL_RC_BLOCK_END))
        {
            in_block = false;
            continue;
        }
        if (in_block)
        {
            // skip lines inside block
            continue;
        }

        fputs(line, tmp);
    }

    fclose(f);
    fclose(tmp);
    if (rename(tmp_path, rc_path) != 0)
    {
        ATL_errlog("Failed to replace %s with cleaned version", rc_path);
        return;
    }
    ATL_dbglog("Finished removing atl PATH lines from %s", rc_path);
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

#define ATL_IS_INSTALLED_LOCK ".atl_is_installed.lock"

atl_i32 atl_config_install(atl_i32 argc, char **argv)
{
    ATL_VOID(argc);
    ATL_VOID(argv);
    if (ATL_isfile(ATL_IS_INSTALLED_LOCK))
    {
        ATL_errlog("It appears that atl is already installed");
        return 1;
    }

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

    // create the .config/atlas_build_manager
    // if does not exists
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

    char local_share_dir[ATL_BUF_SIZE_1024];
    snprintf(local_share_dir, sizeof(local_share_dir), "%s/.local/share/atl", ATL_GET_HOME());
    if (!ATL_isdir(local_share_dir))
    {
        if (atl_mkdir(local_share_dir))
        {
            ATL_errlog("%s: Failed to create %s directory", __func__, local_share_dir);
            return 1;
        }
    }

    // create the .config/atlas_build_manager/atl.conf
    // if does not exists
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
        ATL_dbglog("Global config ensured: %s", config_file);
        ATL_file_append(config_file, "# This file is auto generated by 'atl config --install'\n");

        //----------------------------------------------------------------------------------------------------
        // write to atl.conf

        // add atl root directory
        ATL_file_append(config_file, "%s:str=\"%s\";\n", ATL_GLOB_CONFIG_KEY_ROOT_DIR, ATL_getcwd());

        // write data dir in .local
        char glob_local_share_dir[ATL_BUF_SIZE_1024];
        snprintf(glob_local_share_dir, sizeof(glob_local_share_dir), "%s/.local/share/atl", ATL_GET_HOME());
        ATL_file_append(config_file, "%s:str=\"%s\";\n", ATL_GLOB_CONFIG_KEY_DATA_DIR, glob_local_share_dir);

        // default compiler
        const char *compiler = ATL_is_installed("gcc") ? "gcc" : ATL_is_installed("clang") ? "cland" : "N/A";
        ATL_file_append(config_file, "%s:str=\"%s\";\n", ATL_GLOB_CONFIG_KEY_COMPILER, compiler);

        // linker
        const char *linker = ATL_is_installed("ld.lld")    ? "-fuse-ld=lld"
                             : ATL_is_installed("ld.gold") ? "-fuse-ld=gold"
                                                           : "N/A";
        ATL_file_append(config_file, "%s:str=\"%s\";\n", ATL_GLOB_CONFIG_KEY_LINKER, linker);

        // cflag
        const char *glob_cflags = "";
        ATL_file_append(config_file, "%s:str=\"%s\";\n", ATL_GLOB_CONFIG_KEY_CFLAGS, glob_cflags);

        //----------------------------------------------------------------------------------------------------
    }

    ATL_log("Atlas Build Manager (atl) installed successfully (user-local PATH updated)");
    ATL_log("Please run: source 'path/to/rc' or open a new shell and check $PATH for atl");
    ATL_file_write(ATL_IS_INSTALLED_LOCK, " ");
    return 0;
}

atl_i32 atl_config_uninstall(atl_i32 argc, char **argv)
{
    ATL_VOID(argc);
    ATL_VOID(argv);
    if (atl_access("build/atl_install.lock", R_OK) != 0)
    {
        ATL_errlog("%s: atl config --uninstall must run inside Atlas Build Manager root directory with an "
                   "build/atl_install.lock",
                   __func__);
        return 1;
    }

    if (ATL_isfile(ATL_IS_INSTALLED_LOCK))
    {
        ATL_ConfigTable *rc_table = ATL_config_parse_file(ATL_get_config("file"));
        ATL_ConfigEntry *local_share_dir_entry = ATL_config_table_get(rc_table, ATL_GLOB_CONFIG_KEY_DATA_DIR);
        char *local_share_data_dir = (local_share_dir_entry && local_share_dir_entry->value.str_val)
                                         ? local_share_dir_entry->value.str_val
                                         : NULL;
        if (local_share_data_dir)
        {
            ATL_dbglog("Removing----> %s", local_share_data_dir);
            // remove ~/.local/share/atl directory
            ATL_rmrf(local_share_data_dir);
        }

        // remove .config/atlas_build_manager directory
        if (ATL_isdir(ATL_get_config("dir")))
        {
            ATL_rmrf(ATL_get_config("dir"));
        }

        atl_remove_path_line();
        ATL_rmrf(ATL_IS_INSTALLED_LOCK);
        ATL_log("Atlas Build Manager uninstalled successfully (user-local PATH updated)");
        rc_table->destroy(rc_table);
    }
    else
    {
        ATL_errlog("Does not seem that atl is installed");
        return 1;
    }
    return 0;
}

atl_i32 atl_config_view(atl_i32 argc, char **argv)
{
    ATL_VOID(argc);
    ATL_VOID(argv);
    const char *rc_path = ATL_get_config("file");
    if (!ATL_isfile(rc_path))
    {
        ATL_errlog("File does not exist: %s", rc_path);
        return 1;
    }

    FILE *rc_file = fopen(rc_path, "r");
    if (!rc_file)
    {
        ATL_errlog("Failed to open %s for read", rc_path);
        return 1;
    }

    if (fseek(rc_file, 0, SEEK_END) != 0)
    {
        fclose(rc_file);
        return 1;
    }

    atl_i64 size = ftell(rc_file);
    if (size < 0)
    {
        fclose(rc_file);
        return 1;
    }

    rewind(rc_file);

    char *buffer = malloc(size + 1);  // will use malloc here cause we free inside the function
    if (!buffer)
    {
        ATL_errlog("%s: Failed to allocate memory for %s", __func__, buffer);
        fclose(rc_file);
        return 1;
    }

    size_t read_bytes = fread(buffer, 1, size, rc_file);
    fclose(rc_file);
    if (read_bytes != (size_t) size)
    {
        free(buffer);
        return 1;
    }

    ATL_printf("\n--------------------------------------------------------------------------------\n");
    ATL_printf("Path: %s\n\n", rc_path);
    ATL_printf("%s\n", buffer);
    ATL_printf("\n--------------------------------------------------------------------------------\n");
    return 0;
}

atl_i32 atl_config_edit(atl_i32 argc, char **argv)
{
    ATL_VOID(argc);
    ATL_VOID(argv);

    char cmd_buf[ATL_BUF_SIZE_256];
    snprintf(cmd_buf, sizeof(cmd_buf), "%s %s", ATL_get_config("editor"), ATL_get_config("file"));

    // TODO: change this with execvp and fork in future
    atl_i32 system_err = system(cmd_buf);
    if (system_err)
    {
        ATL_errlog("Failed to run %s system command", cmd_buf);
        return 1;
    }
    return 0;
}

//----------------------------------------------------------------------------------------------------

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
        if (ATL_strmatch(argv[i], "--view"))
        {
            return atl_config_view(argc, argv);
        }
        if (ATL_strmatch(argv[i], "--edit"))
        {
            return atl_config_edit(argc, argv);
        }
    }

    ATL_errlog("%s: Unknown option for config command", __func__);
    return 1;
}
