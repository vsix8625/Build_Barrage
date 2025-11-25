#include "barr_cmd_install.h"
#include "barr_cmd_version.h"
#include "barr_io.h"
#include <stdio.h>
#include <string.h>

static const char *barr_path_filename(const char *path)
{
    if (path == NULL)
    {
        return NULL;
    }

    const char *last = path;
    const char *p = path;

    while (*p)
    {
        if (*p == BARR_PATH_SEPARATOR_CHAR)
        {
            last = p + 1;
        }
        p++;
    }

    return last;
}

static void barr_install_target(const char *install_prefix)
{
    if (install_prefix == NULL)
    {
        return;
    }

    char *info = BARR_DATA_BUILD_INFO_PATH;

    const char *name = BARR_get_build_info_key(info, "name");
    const char *type = BARR_get_build_info_key(info, "type");

    const char *shared_art = BARR_get_build_info_key(info, "shared");
    const char *static_art = BARR_get_build_info_key(info, "static");

    if (name == NULL || type == NULL)
    {
        BARR_errlog("Invalid build info");
        return;
    }

    char src_path[BARR_MATH_DOUBLE(BARR_PATH_MAX)];
    char dst_path[BARR_MATH_DOUBLE(BARR_PATH_MAX)];
    BARR_file_write(BARR_DATA_INSTALL_INFO_PATH, "# %s installed paths\n", name);

    if (BARR_strmatch(type, "executable") || BARR_strmatch(type, "exec"))
    {
        const char *build_dir = BARR_get_build_info_key(info, "build_dir");

        if (build_dir && build_dir[0] != '\0')
        {
            snprintf(src_path, sizeof(src_path), "%s/bin/%s", build_dir, name);
            snprintf(dst_path, sizeof(dst_path), "%s/bin/%s", install_prefix, name);

            BARR_log("Installing executable: %s -> %s", src_path, dst_path);
            if (BARR_fs_copy(src_path, dst_path) != 0)
            {
                BARR_errlog("Failed to copy executable");
                return;
            }
            BARR_file_append(BARR_DATA_INSTALL_INFO_PATH, "%s\n", dst_path);
        }
    }
    else if (BARR_strmatch(type, "shared") && shared_art && shared_art[0] != '\0')
    {
        snprintf(dst_path, sizeof(dst_path), "%s/lib/%s", install_prefix, barr_path_filename(shared_art));
        BARR_log("Installing shared library: %s -> %s", shared_art, dst_path);
        if (BARR_fs_copy(shared_art, dst_path) != 0)
        {
            BARR_errlog("Failed to copy shared library");
            return;
        }
        BARR_file_append(BARR_DATA_INSTALL_INFO_PATH, "%s\n", dst_path);
    }
    else if (BARR_strmatch(type, "static") && static_art && static_art[0] != '\0')
    {
        snprintf(dst_path, sizeof(dst_path), "%s/lib/%s", install_prefix, barr_path_filename(static_art));
        if (BARR_fs_copy(static_art, dst_path) != 0)
        {
            BARR_errlog("Failed to copy static library");
            return;
        }
        BARR_file_append(BARR_DATA_INSTALL_INFO_PATH, "%s\n", dst_path);
    }
    else if (BARR_strmatch(type, "library"))
    {
        if (shared_art && shared_art[0] != '\0')
        {
            snprintf(dst_path, sizeof(dst_path), "%s/lib/%s", install_prefix, barr_path_filename(shared_art));
            BARR_log("Installing shared library: %s -> %s", shared_art, dst_path);
            if (BARR_fs_copy(shared_art, dst_path) != 0)
            {
                BARR_errlog("Failed to copy shared library");
                return;
            }
            BARR_file_append(BARR_DATA_INSTALL_INFO_PATH, "%s\n", dst_path);
        }

        if (static_art && static_art[0] != '\0')
        {
            snprintf(dst_path, sizeof(dst_path), "%s/lib/%s", install_prefix, barr_path_filename(static_art));
            if (BARR_fs_copy(static_art, dst_path) != 0)
            {
                BARR_errlog("Failed to copy static library");
                return;
            }
            BARR_file_append(BARR_DATA_INSTALL_INFO_PATH, "%s\n", dst_path);
        }
    }

    // Public includes
    if (BARR_strmatch(type, "library") || BARR_strmatch(type, "shared") || BARR_strmatch(type, "static"))
    {
        char *cflags_val = BARR_get_build_info_key(info, "cflags");
        if (cflags_val && cflags_val[0] != '\0')
        {
            const char **tokens = (const char **) BARR_tokenize_string(cflags_val);
            for (const char **p = tokens; p && *p; ++p)
            {
                const char *tok = *p;

                if (strncmp(tok, "-I", 2) == 0)
                {
                    const char *raw_path = tok + 2;
                    char resolved[BARR_PATH_MAX];

                    if (BARR_path_resolve(BARR_getcwd(), raw_path, resolved, sizeof(resolved)))
                    {
                        char dst[BARR_MATH_DOUBLE(BARR_PATH_MAX)];
                        snprintf(dst, sizeof(dst), "%s/include/%s/%s", install_prefix, name, raw_path);
                        if (BARR_isdir(raw_path))
                        {
                            BARR_mkdir_p(dst);
                        }
                        BARR_log("Installing include directory: %s -> %s", resolved, dst);
                        if (BARR_fs_copy_tree(resolved, dst) != 0)
                        {
                            BARR_warnlog("Failed to copy include dir: %s", resolved);
                            continue;
                        }
                        BARR_file_append(BARR_DATA_INSTALL_INFO_PATH, "%s\n", dst);
                    }
                    else
                    {
                        BARR_warnlog("Path does not exist, skipping: %s", raw_path);
                    }
                }
            }

            char top_level_include[BARR_MATH_DOUBLE(BARR_PATH_MAX)];
            snprintf(top_level_include, sizeof(top_level_include), "%s/include/%s", install_prefix, name);

            if (BARR_isdir(top_level_include))
            {
                BARR_file_append(BARR_DATA_INSTALL_INFO_PATH, "%s\n", top_level_include);
                BARR_log("Added top-level include dir to manifest: %s", top_level_include);
            }
        }
    }

    BARR_log("Install complete to: %s", install_prefix);
}

// NOTE: WIP
barr_i32 BARR_command_install(barr_i32 argc, char **argv)
{
    if (!BARR_init())
    {
        return 1;
    }

    if (!BARR_isfile(BARRFILE))
    {
        BARR_errlog("Install command requires to read the Barrfile of the target");
        return 1;
    }

    if (getuid() != 0)
    {
        BARR_errlog("[%s]:%s(): sudo required", __TIME__, __func__);
        return 1;
    }

    char *prefix = NULL;
    char *destdir = NULL;

    for (barr_i32 i = 1; i < argc; ++i)
    {
        char *cmd = argv[i];
        if (BARR_strmatch(cmd, "--prefix"))
        {
            if (i + 1 >= argc)
            {
                BARR_errlog("Option --prefix requires <dir_path>");
                return 1;
            }
            prefix = argv[i + 1];
            BARR_log("Prefix path set to: %s", prefix);
            i++;
        }
        else if (BARR_strmatch(cmd, "--destdir"))
        {
            if (i + 1 >= argc)
            {
                BARR_errlog("Option --destdir requires <dir_path>");
                return 1;
            }
            destdir = argv[i + 1];
            BARR_log("Destdir path set to: %s", destdir);
            i++;
        }
        else
        {
            BARR_warnlog("Invalid option for install command: %s", cmd);
            return 1;
        }
    }

    if (prefix == NULL)
    {
        prefix = "/usr/local";
        BARR_log("Prefix defaulted to: %s", prefix);
    }

    if (destdir == NULL)
    {
        destdir = "";
        BARR_log("Destdir defaulted to empty (direct install under prefix)");
    }

    char target_path[BARR_PATH_MAX];
    snprintf(target_path, sizeof(target_path), "%s%s", destdir, prefix);

    barr_install_target(target_path);

    return 0;
}
