#include "barr_cmd_new.h"
#include "barr_cmd_new_create_project.h"
#include "barr_io.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DASH       '-'
#define UNDERSCORE '_'
#define MAX_LEN    128

bool barr_is_valid_name(const char *str);
bool barr_is_valid_file_name(const char *str);

//----------------------------------------------------------------------------------------------------

barr_i32 barr_create_pair_files(const char *name,
                                const char *dir,
                                const char *ext,
                                const char *header_ext,
                                bool        is_public);

//----------------------------------------------------------------------------------------------------

barr_i32 BARR_command_new(barr_i32 argc, char **argv)
{
    if (argc < 2)
    {
        BARR_errlog("barr new command requires option");
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
            if (!BARR_init())
            {
                return 1;
            }
            if (i + 1 >= argc)
            {
                BARR_errlog("barr new --file requires <file_name>");
                return 1;
            }

            const char *file_name = argv[i + 1];
            const char *dir_path  = ".";  // current dir def

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
                BARR_log("File created: %s", fullpath);
            }

            i++;
        }
        else if (BARR_strmatch(arg, "--barrfile"))
        {
            if (!BARR_init())
            {
                return 1;
            }

            if (!BARR_isfile("Barrfile"))
            {
                char *barrfile_contents =
                    "# ------------------------------------------------------------------\n"
                    "#\n"
                    "#   • All constant values must be written in ALL_CAPS_WITH_UNDERSCORES.\n"
                    "#   • lowercase / camelCase      → Your custom variables (free to define)\n"
                    "#   • Lines that start with '#' are comments and are ignored.\n"
                    "#   • Use ${VAR_NAME} to reference another variable.\n"
                    "#   • Variables not defined keep their built-in defaults.\n"
                    "#\n"
                    "# ------------------------------------------------------------------\n"
                    "\n"
                    "print(\"Barrfile execution started\");\n"
                    "\n"
                    "# ------------------------------------------------------------------\n"
                    "# 1. PROJECT METADATA                                                \n"
                    "# ------------------------------------------------------------------\n"
                    "TARGET        = \"barr_default\";          # Name of the final binary / "
                    "library\n"
                    "VERSION       = \"0.0.1\";                # Project version (informational)\n"
                    "TARGET_TYPE   = \"executable\";           # \"executable\" | \"static\" | "
                    "\"shared\"\n"
                    "\n"
                    "print(\"Building ${TARGET}-v${VERSION} | Type: ${TARGET_TYPE}\");\n"
                    "\n"
                    "# ------------------------------------------------------------------\n"
                    "# 2. TOOLCHAIN                                                       \n"
                    "# ------------------------------------------------------------------\n"
                    "COMPILER      = \"/usr/bin/clang\";  # C compiler (full path recommended)\n"
                    "LINKER        = \"lld\";             # Linker (ld, gold, lld,mold …)\n"
                    "\n"
                    "# ------------------------------------------------------------------\n"
                    "# 3. BUILD TYPE & OUTPUT                                             \n"
                    "# ------------------------------------------------------------------\n"
                    "BUILD_TYPE    = \"debug\";                          # \"debug\" (default) | "
                    "\"release\"\n"
                    "OUT_DIR       = \"build/${TARGET}/${BUILD_TYPE}\";  # Directory for objects & "
                    "final binary\n"
                    "\n"
                    "# ------------------------------------------------------------------\n"
                    "# 4. COMPILATION FLAGS                                               \n"
                    "# ------------------------------------------------------------------\n"
                    "# You can define your own variables like the line below.\n"
                    "std           = \"-std=c23\";             # C standard (c11, c17, c23, …)\n"
                    "CFLAGS        = \"${std} -Wall -Werror -Wextra -g\";   # Debug flags\n"
                    "CFLAGS_RELEASE = \"${std} -Wall -O3\";                # Release flags (used "
                    "when "
                    "BUILD_TYPE=release)\n"
                    "\n"
                    "# ------------------------------------------------------------------\n"
                    "# 5. SOURCE & INCLUDE DISCOVERY                                      \n"
                    "# ------------------------------------------------------------------\n"
                    "# AUTO_INCLUDE_DISCOVERY = \"on\";   # \"on\" (default) | \"append\" | "
                    "\"off\"\n"
                    "#   • \"on\"  – automatically scan the project root for headers.\n"
                    "#   • \"append\" – scan + keep manually added INCLUDES.\n"
                    "#   • \"off\" – disable scanning; only use explicit INCLUDES.\n"
                    "#\n"
                    "# INCLUDES = \"-Iinc\";             # Manually added include directories\n"
                    "#   • Adding this variable automatically sets AUTO_INCLUDE_DISCOVERY = "
                    "\"off\".\n"
                    "\n"
                    "# GLOB_SOURCES = \"src dir1 dir2\"; # Recursive glob patterns (turns off "
                    "auto-source discovery)\n"
                    "# SOURCES = \"main.c file1.c src/file3.c\"; # Explicit source list\n"
                    "\n"
                    "# ------------------------------------------------------------------\n"
                    "# 6. PREPROCESSOR DEFINES                                            \n"
                    "# ------------------------------------------------------------------\n"
                    "DEFINES       = \"-DDEBUG\";              # Additional -D flags\n"
                    "\n"
                    "# ------------------------------------------------------------------\n"
                    "# 7. LINKING & LIBRARIES                                             \n"
                    "# ------------------------------------------------------------------\n"
                    "# LFLAGS      = \"-lpthread\";            # Linker flags\n"
                    "# LIB_PATHS   = \"-L/usr/local/lib\";     # Library search paths\n"
                    "\n"
                    "# ------------------------------------------------------------------\n"
                    "# 8. MISCELLANEOUS                                                   \n"
                    "# ------------------------------------------------------------------\n"
                    "# GEN_COMPILE_COMMANDS = \"on\";           # Generate compile_commands.json "
                    "(on/off)\n"
                    "# EXCLUDE_PATTERNS = \"build test\";        # Patterns excluded from glob "
                    "scanning\n"
                    "\n"
                    "# ------------------------------------------------------------------\n"
                    "# 9. MODULE SUPPORT\n"
                    "# ------------------------------------------------------------------\n"
                    "# To build a module for ${TARGET} you can add this function.\n"
                    "# barr build will run it, before building the ${TARGET}\n"
                    "# add_module(\"engine\",\"engine\",\"required\");\n"
                    "# engine_includes = \"-Iinclude -Iengine_public_inc\";\n"
                    "# engine_clean_targets = \"engine/build engine/.barr/cache/build.cache\";\n\n"
                    "# Finalize the module includes in case of multiple modules\n"
                    "# MODULES_INCLUDES = \"${engine_includes}\"; # Aggregated includes from "
                    "modules\n"
                    "# CLEAN_TARGETS = \"${engine_clean_targets} ${other_clean_targets}...\";      "
                    " # Extra paths "
                    "removed by 'barr clean'\n"
                    "\n"
                    "# ------------------------------------------------------------------\n"
                    "# 10. PACKAGES SUPPORT\n"
                    "# ------------------------------------------------------------------\n"
                    "# Finding a system package eg: \"xxhash\" and \"zlib\"\n"
                    "# pkgs = \"xxhash zlib\";\n"
                    "# find_package(\"${pkgs}\");\n"
                    "\n"
                    "# ------------------------------------------------------------------\n"
                    "# End of Barrfile                                                    \n"
                    "# ------------------------------------------------------------------\n"
                    "\n"
                    "print(\"Barrfile execution finished\");\n";

                BARR_file_write(
                    "Barrfile",
                    "# Auto-generated by Build Barrage v%d.%d.%d\n# Latest update: %s\n\n",
                    BARR_VERSION_MAJOR,
                    BARR_VERSION_MINOR,
                    BARR_VERSION_PATCH,
                    BARR_VERSION_DATE);
                BARR_file_append("Barrfile", "%s", barrfile_contents);
                BARR_log(
                    "Barrfile created, you can run 'barr config open' to open it with you $EDITOR");
            }
            else
            {
                BARR_warnlog("Barrfile already exists in '%s'", BARR_getcwd());
            }
            break;
        }
        else if (BARR_strmatch(arg, "--main"))
        {
            if (!BARR_init())
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
                                      "int main(void){\n"
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
        else if (BARR_strmatch(arg, "--pair"))
        {
            if (!BARR_init())
            {
                return 1;
            }

            const char *name = argv[++i];
            if (name == NULL)
            {
                BARR_errlog("Missing name for --pair");
                return 1;
            }

            const char *dir        = "src";
            const char *ext        = ".c";
            const char *header_ext = ".h";
            bool        is_public  = false;

            while (i + 1 < argc)
            {
                const char *next = argv[i + 1];
                if (BARR_strmatch(next, "--dir"))
                {
                    dir  = argv[i + 2];
                    i   += 2;
                }
                else if (BARR_strmatch(next, "--ext"))
                {
                    ext = argv[i + 2];
                    if (BARR_strmatch(ext, ".cpp"))
                    {
                        header_ext = ".hpp";
                    }
                    i += 2;
                }
                else if (BARR_strmatch(next, "--public"))
                {
                    is_public  = BARR_strmatch(argv[i + 2], "true");
                    i         += 2;
                }
                else
                {
                    break;
                }
            }

            barr_create_pair_files(name, dir, ext, header_ext, is_public);
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
        BARR_warnlog(
            "Project name length: %zu is greater than max allowed: %d", strlen(str), MAX_LEN);
        return false;
    }

    if (!isalnum((unsigned char) str[0]))
    {
        BARR_warnlog("Name should start with [Aa-Zz|0-9] not with: %c", str[0]);
        return false;
    }

    for (size_t i = 1; i < len; ++i)
    {
        char c    = str[i];
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
        char c    = str[i];
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

//----------------------------------------------------------------------------------------------------
barr_i32 barr_create_pair_files(const char *name,
                                const char *dir,
                                const char *ext,
                                const char *header_ext,
                                bool        is_public)
{
    if (name == NULL || dir == NULL || ext == NULL || header_ext == NULL)
    {
        return 1;
    }

    if (!BARR_isdir(dir))
    {
        BARR_mkdir_p(dir);
    }

    const char *header_dir = dir;
    if (is_public)
    {
    }
    if (is_public)
    {
        header_dir = "include";
        if (!BARR_isdir(header_dir))
        {
            BARR_mkdir_p(header_dir);
        }
    }

    char source_path[BARR_BUF_SIZE_4096];
    char header_path[BARR_BUF_SIZE_4096];

    snprintf(source_path, sizeof(source_path), "%s/%s%s", dir, name, ext);
    snprintf(header_path, sizeof(header_path), "%s/%s%s", header_dir, name, header_ext);

    if (!BARR_isfile(header_path))
    {
        char guard[BARR_PATH_MAX];
        snprintf(guard, sizeof(guard), "%s_H_", name);

        for (char *p = guard; *p; ++p)
        {
            *p = (char) toupper(*p);
        }

        char header_contents[(sizeof(guard) * 3) + 64];
        snprintf(header_contents,
                 sizeof(header_contents),
                 "#ifndef %s\n"
                 "#define %s\n\n"
                 "#endif // %s\n",
                 guard,
                 guard,
                 guard);

        BARR_file_write(header_path, "%s", header_contents);
        BARR_log("Created: %s", header_path);
    }
    else
    {
        BARR_warnlog("%s already exists", header_path);
    }

    if (!BARR_isfile(source_path))
    {
        char src_contents[BARR_BUF_SIZE_16K];
        snprintf(src_contents,
                 sizeof(src_contents),
                 "// Created by Build Barrage\n"
                 "#include \"%s%s\"\n\n",
                 name,
                 header_ext);

        BARR_file_write(source_path, "%s", src_contents);
        BARR_log("Created: %s", source_path);
    }
    else
    {
        BARR_warnlog("%s already exists", source_path);
    }

    return 0;
}
