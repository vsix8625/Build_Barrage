#include "barr_cmd_config_linux.h"
#include "barr_debug.h"
#include "barr_env.h"
#include "barr_glob_config_keys.h"
#include "barr_glob_config_parser.h"
#include "barr_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *barr_get_bin_path(void);

static const char *barr_get_shell_rc(char *buf, size_t buf_size)
{
    const char *shell = getenv("SHELL");
    if (!shell)
    {
        shell = "/bin/sh";
    }

    const char *shell_name = strrchr(shell, '/');
    shell_name = shell_name ? shell_name + 1 : shell;

    if (BARR_strmatch(shell_name, "bash"))
    {
        snprintf(buf, buf_size, "%s/.bashrc", BARR_GET_HOME());
    }
    else if (BARR_strmatch(shell_name, "zsh"))
    {
        snprintf(buf, buf_size, "%s/.zshrc", BARR_GET_HOME());
    }
    else if (BARR_strmatch(shell_name, "fish"))
    {
        snprintf(buf, buf_size, "%s/.config/fish/config.fish", BARR_GET_HOME());
    }
    else
    {
        snprintf(buf, buf_size, "%s/.profile", BARR_GET_HOME());  // fallback
    }

    return buf;
}

#define BARR_RC_BLOCK_BEGIN "# >>> Build Barrage >>>"
#define BARR_RC_BLOCK_END "# <<< Build Barrage <<<"

static void barr_append_path_line(const char *barr_bin_path)
{
    char rc_path[BARR_BUF_SIZE_512];
    barr_get_shell_rc(rc_path, sizeof(rc_path));

    FILE *check = fopen(rc_path, "r");
    if (check)
    {
        char line[BARR_BUF_SIZE_1024];
        while (fgets(line, sizeof(line), check))
        {
            if (strstr(line, BARR_RC_BLOCK_BEGIN))
            {
                fclose(check);
                BARR_log("Barr block already present in %s", rc_path);
                return;  // already installed
            }
        }
        fclose(check);
    }

    FILE *f = fopen(rc_path, "a");
    if (!f)
    {
        BARR_errlog("Failed to open %s for appending", rc_path);
        return;
    }

    fprintf(f,
            "\n%s\ncase \":$PATH:\" in\n"
            " *\":%s:\"*) ;;\n"
            " *) PATH=\"$PATH:%s\" ;;\n"
            "esac\n"
            "export PATH\n"
            "%s\n",
            BARR_RC_BLOCK_BEGIN, barr_bin_path, barr_bin_path, BARR_RC_BLOCK_END);
    fclose(f);

    BARR_log("Added barr PATH block in %s", rc_path);
}

static void barr_remove_path_line(void)
{
    char rc_path[BARR_BUF_SIZE_512];
    barr_get_shell_rc(rc_path, sizeof(rc_path));

    // make a backup
    char backup_path[BARR_BUF_SIZE_1024];
    snprintf(backup_path, sizeof(backup_path), "%s.bak", rc_path);
    if (BARR_file_copy(rc_path, backup_path) == 0)
    {
        BARR_log("Backup created: %s", backup_path);
    }
    else
    {
        BARR_warnlog("Could not create backup of %s", rc_path);
    }

    FILE *f = fopen(rc_path, "r");
    if (!f)
    {
        BARR_errlog("Failed to open %s for read", rc_path);
        return;
    }

    char tmp_path[BARR_BUF_SIZE_512];
    snprintf(tmp_path, sizeof(tmp_path), "%s/.tmp_barr_rc", BARR_GET_HOME());

    FILE *tmp = fopen(tmp_path, "w");
    if (!tmp)
    {
        fclose(f);
        BARR_errlog("Failed to open %s for read", tmp_path);
        return;
    }

    char line[BARR_BUF_SIZE_1024];
    bool in_block = false;

    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, BARR_RC_BLOCK_BEGIN))
        {
            in_block = true;
            continue;  // skip begin marker
        }

        if (strstr(line, BARR_RC_BLOCK_END))
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
        BARR_errlog("Failed to replace %s with cleaned version", rc_path);
        return;
    }
    BARR_dbglog("Finished removing barr PATH lines from %s", rc_path);
}

#define BARR_BIN_PATH "build/path_to_bin_dir.barr"

static const char *barr_get_bin_path(void)
{
    static char buf[BARR_BUF_SIZE_512];
    const char *tmp_path = BARR_BIN_PATH;
    FILE *f = fopen(tmp_path, "r");
    if (!f)
    {
        BARR_errlog("%s: Failed to open %s to read", __func__, tmp_path);
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

#define BARR_IS_INSTALLED_LOCK ".barr_is_installed.lock"

barr_i32 barr_config_install(barr_i32 argc, char **argv)
{
    BARR_VOID(argc);
    BARR_VOID(argv);
    if (BARR_isfile(BARR_IS_INSTALLED_LOCK))
    {
        BARR_errlog("It appears that Build Barrage is already installed");
        return 1;
    }

    if (barr_access("build/barr_install.lock", R_OK) != 0)
    {
        BARR_errlog(
            "%s: barr config --install must run inside Build Barrage root directory with an build/barr_install.lock",
            __func__);
        return 1;
    }

    const char *barr_bin_path = barr_get_bin_path();
    if (!barr_bin_path)
    {
        BARR_errlog("Could not locate bin_fullpath.barr. Did you run ./scripts/build first");
        return 1;
    }

    barr_append_path_line(barr_bin_path);

    // create the .config/build_barrage
    // if does not exists
    char config_dir[BARR_BUF_SIZE_512];
    snprintf(config_dir, sizeof(config_dir), "%s", BARR_get_config("dir"));
    if (!BARR_isdir(config_dir))
    {
        if (barr_mkdir(config_dir))
        {
            BARR_errlog("%s: Failed to create %s directory", __func__, config_dir);
            return 1;
        }
    }

    char local_share_dir[BARR_BUF_SIZE_1024];
    snprintf(local_share_dir, sizeof(local_share_dir), "%s/.local/share/barr", BARR_GET_HOME());
    if (!BARR_isdir(local_share_dir))
    {
        if (barr_mkdir(local_share_dir))
        {
            BARR_errlog("%s: Failed to create %s directory", __func__, local_share_dir);
            return 1;
        }
        char share_modes_dir[BARR_PATH_MAX];
        snprintf(share_modes_dir, sizeof(share_modes_dir), "%s/.local/share/barr/modes", BARR_GET_HOME());
        if (barr_mkdir(share_modes_dir))
        {
            BARR_errlog("%s: Failed to create %s directory", __func__, share_modes_dir);
            return 1;
        }
    }

    // create the .config/build_barrage/barr.conf
    // if does not exists
    char config_file[BARR_BUF_SIZE_512];
    snprintf(config_file, sizeof(config_file), "%s", BARR_get_config("file"));
    if (!BARR_isfile(config_file))
    {
        FILE *f = fopen(config_file, "a");
        if (!f)
        {
            BARR_errlog("%s: Failed to create global config: %s", __func__, config_file);
            return 1;
        }
        fclose(f);
        BARR_dbglog("Global config ensured: %s", config_file);
        BARR_file_append(config_file, "# This file is auto generated by 'barr config --install'\n");

        //----------------------------------------------------------------------------------------------------
        // write to barr.conf

        // add barr root directory
        BARR_file_append(config_file, "%s:str=\"%s\";\n", BARR_GLOB_CONFIG_KEY_ROOT_DIR, BARR_getcwd());

        // write data dir in .local
        char glob_local_share_dir[BARR_BUF_SIZE_1024];
        snprintf(glob_local_share_dir, sizeof(glob_local_share_dir), "%s/.local/share/barr", BARR_GET_HOME());
        BARR_file_append(config_file, "%s:str=\"%s\";\n", BARR_GLOB_CONFIG_KEY_DATA_DIR, glob_local_share_dir);

        // default compiler
        const char *compiler = BARR_is_installed("gcc") ? "gcc" : BARR_is_installed("clang") ? "cland" : "N/A";
        BARR_file_append(config_file, "%s:str=\"%s\";\n", BARR_GLOB_CONFIG_KEY_COMPILER, compiler);

        // linker
        const char *linker = BARR_is_installed("ld.lld")    ? "-fuse-ld=lld"
                             : BARR_is_installed("ld.gold") ? "-fuse-ld=gold"
                                                            : "N/A";
        BARR_file_append(config_file, "%s:str=\"%s\";\n", BARR_GLOB_CONFIG_KEY_LINKER, linker);

        // cflag
        const char *glob_cflags = "";
        BARR_file_append(config_file, "%s:str=\"%s\";\n", BARR_GLOB_CONFIG_KEY_CFLAGS, glob_cflags);

        //----------------------------------------------------------------------------------------------------
    }

    BARR_log("Build Barrage (barr) installed successfully (user-local PATH updated)");
    BARR_log("Please run: source 'path/to/rc' or open a new shell and check $PATH for barr");
    BARR_file_write(BARR_IS_INSTALLED_LOCK, " ");
    return 0;
}

barr_i32 barr_config_uninstall(barr_i32 argc, char **argv)
{
    BARR_VOID(argc);
    BARR_VOID(argv);
    if (barr_access("build/barr_install.lock", R_OK) != 0)
    {
        BARR_errlog("%s: barr config --uninstall must run inside Build Barrage root directory with an "
                    "build/barr_install.lock",
                    __func__);
        return 1;
    }

    if (BARR_isfile(BARR_IS_INSTALLED_LOCK))
    {
        BARR_ConfigTable *rc_table = BARR_config_parse_file(BARR_get_config("file"));
        BARR_ConfigEntry *local_share_dir_entry = BARR_config_table_get(rc_table, BARR_GLOB_CONFIG_KEY_DATA_DIR);
        char *local_share_data_dir = (local_share_dir_entry && local_share_dir_entry->value.str_val)
                                         ? local_share_dir_entry->value.str_val
                                         : NULL;
        if (local_share_data_dir)
        {
            BARR_dbglog("Removing----> %s", local_share_data_dir);
            // remove ~/.local/share/barr directory
            BARR_rmrf(local_share_data_dir);
        }

        // remove .config/build_barrage directory
        if (BARR_isdir(BARR_get_config("dir")))
        {
            BARR_rmrf(BARR_get_config("dir"));
        }

        barr_remove_path_line();
        BARR_rmrf(BARR_IS_INSTALLED_LOCK);
        BARR_log("Build Barrage uninstalled successfully (user-local PATH updated)");
        rc_table->destroy(rc_table);
    }
    else
    {
        BARR_errlog("Does not seem that barr is installed");
        return 1;
    }
    return 0;
}

barr_i32 barr_config_view(barr_i32 argc, char **argv)
{
    BARR_VOID(argc);
    BARR_VOID(argv);
    const char *rc_path = BARR_get_config("file");
    if (!BARR_isfile(rc_path))
    {
        BARR_errlog("File does not exist: %s", rc_path);
        return 1;
    }

    FILE *rc_file = fopen(rc_path, "r");
    if (!rc_file)
    {
        BARR_errlog("Failed to open %s for read", rc_path);
        return 1;
    }

    if (fseek(rc_file, 0, SEEK_END) != 0)
    {
        fclose(rc_file);
        return 1;
    }

    barr_i64 size = ftell(rc_file);
    if (size < 0)
    {
        fclose(rc_file);
        return 1;
    }

    rewind(rc_file);

    char *buffer = malloc(size + 1);  // will use malloc here cause we free inside the function
    if (!buffer)
    {
        BARR_errlog("%s: Failed to allocate memory", __func__);
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

    BARR_printf("\n--------------------------------------------------------------------------------\n");
    BARR_printf("Path: %s\n\n", rc_path);
    BARR_printf("%s\n", buffer);
    BARR_printf("\n--------------------------------------------------------------------------------\n");
    return 0;
}

barr_i32 barr_config_edit(barr_i32 argc, char **argv)
{
    BARR_VOID(argc);
    BARR_VOID(argv);

    char cmd_buf[BARR_BUF_SIZE_256];
    snprintf(cmd_buf, sizeof(cmd_buf), "%s %s", BARR_get_config("editor"), BARR_get_config("file"));

    char *args[] = {(char *) BARR_get_config("editor"), (char *) BARR_get_config("file"), NULL};
    BARR_run_process(args[0], args, false);
    return 0;
}

//----------------------------------------------------------------------------------------------------

barr_i32 BARR_cmd_config(barr_i32 argc, char **argv)
{
    if (argc < 2)
    {
        BARR_errlog("%s: config command requires option", __func__);
        return 1;
    }

    for (barr_i32 i = 1; i < argc; ++i)
    {
        if (BARR_strmatch(argv[i], "--install"))
        {
            return barr_config_install(argc, argv);
        }
        if (BARR_strmatch(argv[i], "--uninstall"))
        {
            return barr_config_uninstall(argc, argv);
        }
        if (BARR_strmatch(argv[i], "--view"))
        {
            return barr_config_view(argc, argv);
        }
        if (BARR_strmatch(argv[i], "--edit"))
        {
            return barr_config_edit(argc, argv);
        }
    }

    BARR_errlog("%s: Unknown option for config command", __func__);
    return 1;
}
