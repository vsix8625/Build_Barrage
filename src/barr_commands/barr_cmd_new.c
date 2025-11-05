#include "barr_cmd_new.h"
#include "barr_cmd_new_create_project.h"
#include "barr_io.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DASH '-'
#define UNDERSCORE '_'
#define MAX_LEN 128

bool barr_is_valid_name(const char *str);
bool barr_is_valid_file_name(const char *str);

//----------------------------------------------------------------------------------------------------

barr_i32 BARR_command_new(barr_i32 argc, char **argv)
{
    if (argc < 2)
    {
        BARR_errlog("new requires option starting with '--'");
        BARR_errlog("Usage: barr new <opt> <arg>");
        return 1;
    }

    for (barr_i32 i = 1; i < argc; i++)
    {
        char *arg = argv[i];

        if (BARR_strmatch(arg, "--project") || BARR_strmatch(arg, "-p"))
        {
            if (i + 1 >= argc)
            {
                BARR_errlog("new --project requires <project_name>");
                return 1;
            }
            if (barr_is_valid_name(argv[i + 1]))
            {
                return BARR_create_project(argv[i + 1]);
            }
            else
            {
                BARR_errlog("Failed to create %s project directory", argv[i + 1]);
                return 1;
            }
        }
        else if (BARR_strmatch(arg, "--file"))
        {
            if (!BARR_is_initialized())
            {
                return 1;
            }
            if (i + 1 >= argc)
            {
                BARR_errlog("new --file requires <file_name>");
                return 1;
            }

            const char *file_name = argv[i + 1];
            const char *dir_path = ".";  // current dir def

            if (i + 2 < argc && argv[i + 2][0] != '-')
            {
                dir_path = argv[i + 2];
                i++;
            }

            if (!barr_is_valid_file_name(file_name))
            {
                BARR_errlog("Failed to create %s file", file_name);
                return 1;
            }

            char fullpath[BARR_PATH_MAX];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dir_path, file_name);

            if (BARR_file_write(fullpath, " ") > 0)
            {
                BARR_log("Create new file: %s", fullpath);
            }

            i++;
        }
        else if (BARR_strmatch(arg, "--barrfile"))
        {
            if (!BARR_is_initialized())
            {
                return 1;
            }

            if (!BARR_isfile("Barrfile"))
            {
                char *barrfile_contents =
                    "print(\"Barrfile starts!\");\n\n"
                    "# run_cmd(\"echo you can run a shell cmd\");\n"
                    "# Extra clean targets for barr clean\n"
                    "# clean_targets = \"\";\n"
                    "target = \"barr_default\";\n"
                    "version = \"0.0.1\";\n"
                    "target_type = \"executable\";\n\n"
                    "print(\"Building ${target}-v${version} | Type: ${target_type}\");\n"
                    "compiler = \"/usr/bin/gcc\";\n"
                    "# linker = \"lld\";\n"
                    "# If build type is not specified it, defaults to debug\n"
                    "build_type = \"debug\";\n"
                    "# out_dir = \"build/${build_type}/obj\";\n"
                    "# You can expand variables \n"
                    "std = \"-std=c23\";\n"
                    "cflags = \"${std} -Wall -Werror -Wextra -g\";\n"
                    "cflags_release = \"${std} -Wall -O3\";\n"
                    "# Include dirs discovery is turned on by default\n"
                    "# Other modes are: \"append\", \"off\"\n"
                    "# auto_include_discovery = \"on\";\n"
                    "# Manually adding the includes without specifying mode will,\n"
                    "# also turn off auto_include_discovery\n"
                    "# includes = \"-Iinc\";\n"
                    "# sources = \"main.c file1.c src/file3.c\";\n"
                    "# Same as with includes scan it will turn off auto discovery,\n"
                    "# if glob_sources are defined\n"
                    "# IMPORTANT: A single glob is recursive, subdirectories not need to be repeated\n"
                    "# glob_sources = \"src dir1 dir2\";\n"
                    "defines = \"-DDEBUG\";\n\n"
                    "# pkgs = \"xxhash\";\n"
                    "# find_package(\"${pkgs}\");\n"
                    "# lflags = \"-lpthread\";\n"
                    "# lib_paths = \"\";\n"
                    "print(\"Build script ends!\");\n";
                BARR_file_write("Barrfile", "# Default Barrfile - Build Barrage v%d.%d.%d\n# Latest update: %s\n\n",
                                BARR_VERSION_MAJOR, BARR_VERSION_MINOR, BARR_VERSION_PATCH, BARR_VERSION_DATE);
                BARR_file_append("Barrfile", "%s", barrfile_contents);
                BARR_log("Barrfile created, you can run 'barr config --local' to open it with you $EDITOR");
            }
            else
            {
                BARR_warnlog("Barrfile already exists in '%s'", BARR_getcwd());
            }
            break;
        }
        else if (BARR_strmatch(arg, "--main"))
        {
            if (!BARR_is_initialized())
            {
                return 1;
            }
            if (!BARR_isdir("src"))
            {
                barr_mkdir("src");
            }

            if (!BARR_isfile("src/main.c"))
            {
                char *main_contents = "// Created by Build Barrage\n"
                                      "#include <stdio.h>\n\n"
                                      "int main(int argc,char **argv){\n"
                                      "  (void)argc;\n"
                                      "  (void)argv;\n"
                                      "  printf(\"Hello, from barr\\n\");\n"
                                      "  return 0;\n"
                                      "}\n";
                BARR_file_write("src/main.c", "%s", main_contents);
                BARR_log("Created src/main.c");
            }
            else
            {
                BARR_warnlog("main.c already exists");
            }
            break;
        }
        else
        {
            BARR_errlog("Unknown option: %s", arg);
            return 1;
        }
    }

    return 0;
}

//----------------------------------------------------------------------------------------------------

bool barr_is_valid_name(const char *str)
{
    if (!str)
    {
        return false;
    }

    size_t len = strlen(str);
    if (len > (size_t) MAX_LEN)
    {
        BARR_warnlog("Project name length: %zu is greater than max allowed: %d", strlen(str), MAX_LEN);
        return false;
    }

    if (!isalnum((unsigned char) str[0]))
    {
        BARR_warnlog("Name should start with [Aa-Zz|0-9] not with: %c", str[0]);
        return false;
    }

    for (size_t i = 1; i < len; ++i)
    {
        char c = str[i];
        char prev = str[i - 1];

        if (!(isalnum((unsigned char) str[i]) || str[i] == DASH || str[i] == UNDERSCORE))
        {
            BARR_warnlog("Character not allowed: %c", c);
            return false;
        }

        if ((c == DASH && prev == DASH) || (c == UNDERSCORE && prev == UNDERSCORE))
        {
            BARR_warnlog("Double dashes '-' or underscores '_' are not allowed");
            return false;
        }

        if ((c == DASH && prev == UNDERSCORE) || (c == UNDERSCORE && prev == DASH))
        {
            BARR_warnlog("Dash '-' followed by underscore '_' or vice versa are not allowed");
            return false;
        }
    }

    char last = str[len - 1];
    if (last == DASH || last == UNDERSCORE)
    {
        BARR_warnlog("Ending project name with dash '-' or underscore '_' are not allowed");
        return false;
    }

    return true;
}

bool barr_is_valid_file_name(const char *str)
{
    if (!str || !*str)
        return false;

    size_t len = strlen(str);
    if (len > (size_t) MAX_LEN)
    {
        BARR_warnlog("File name length %zu exceeds max allowed %d", len, MAX_LEN);
        return false;
    }

    // Special case: hidden files starting with '.'
    size_t start = 0;
    if (str[0] == '.')
    {
        start = 1;
        if (len == 1)
        {
            BARR_warnlog("Filename cannot be just '.'");
            return false;
        }
    }

    bool dot_seen = false;

    for (size_t i = start; i < len; ++i)
    {
        char c = str[i];
        char prev = (i > 0) ? str[i - 1] : 0;

        if (c == '.')
        {
            if (dot_seen)
            {
                BARR_warnlog("Multiple dots not allowed");
                return false;  // allow only one dot
            }
            if (i == start || i == len - 1)
            {
                BARR_warnlog("Dot cannot start or end the filename");
                return false;
            }
            dot_seen = true;
            continue;
        }

        if (!(isalnum((unsigned char) c) || c == DASH || c == UNDERSCORE))
        {
            BARR_warnlog("Invalid character: %c", c);
            return false;
        }

        // prevent consecutive special chars
        if ((c == DASH && prev == DASH) || (c == UNDERSCORE && prev == UNDERSCORE) ||
            (c == DASH && prev == UNDERSCORE) || (c == UNDERSCORE && prev == DASH))
        {
            BARR_warnlog("Consecutive or mixed '-' and '_' not allowed");
            return false;
        }
    }

    return true;
}
