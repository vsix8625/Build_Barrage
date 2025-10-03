#include "atl_cmd_new.h"
#include "atl_create_project.h"
#include "atl_io.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DASH '-'
#define UNDERSCORE '_'
#define MAX_LEN 128

bool atl_is_valid_name(const char *str);
bool atl_is_valid_file_name(const char *str);

//----------------------------------------------------------------------------------------------------

atl_i32 ATL_command_new(atl_i32 argc, char **argv)
{
    if (argc < 2)
    {
        ATL_errlog("new requires option starting with '--'");
        ATL_errlog("Usage: atl new <opt> <arg>");
        return 1;
    }

    for (atl_i32 i = 1; i < argc; i++)
    {
        char *arg = argv[i];

        if (ATL_strmatch(arg, "--project") || ATL_strmatch(arg, "-p"))
        {
            if (i + 1 >= argc)
            {
                ATL_errlog("new --project requires <project_name>");
                return 1;
            }
            if (atl_is_valid_name(argv[i + 1]))
            {
                return ATL_create_project(argv[i + 1]);
            }
            else
            {
                ATL_errlog("Failed to create %s project directory", argv[i + 1]);
                return 1;
            }
            i++;
        }
        else if (ATL_strmatch(arg, "--file"))
        {
            if (!ATL_is_initialized())
            {
                return 1;
            }
            if (i + 1 >= argc)
            {
                ATL_errlog("new --file requires <file_name>");
                return 1;
            }

            const char *file_name = argv[i + 1];
            const char *dir_path = ".";  // current dir def

            if (i + 2 < argc && argv[i + 2][0] != '-')
            {
                dir_path = argv[i + 2];
                i++;
            }

            if (!atl_is_valid_file_name(file_name))
            {
                ATL_errlog("Failed to create %s file", file_name);
                return 1;
            }

            char fullpath[ATL_PATH_MAX];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dir_path, file_name);

            if (ATL_file_write(fullpath, " ") > 0)
            {
                ATL_log("Create new file: %s", fullpath);
            }

            i++;
        }
        else
        {
            ATL_errlog("Unknown option: %s", arg);
            return 1;
        }
    }

    return 0;
}

//----------------------------------------------------------------------------------------------------

bool atl_is_valid_name(const char *str)
{
    if (!str)
    {
        return false;
    }

    size_t len = strlen(str);
    if (len > (size_t) MAX_LEN)
    {
        ATL_warnlog("Project name length: %zu is greater than max allowed: %d", strlen(str), MAX_LEN);
        return false;
    }

    if (!isalnum((unsigned char) str[0]))
    {
        ATL_warnlog("Name should start with [Aa-Zz|0-9] not with: %c", str[0]);
        return false;
    }

    for (size_t i = 1; i < len; ++i)
    {
        char c = str[i];
        char prev = str[i - 1];

        if (!(isalnum((unsigned char) str[i]) || str[i] == DASH || str[i] == UNDERSCORE))
        {
            ATL_warnlog("Character not allowed: %c", c);
            return false;
        }

        if ((c == DASH && prev == DASH) || (c == UNDERSCORE && prev == UNDERSCORE))
        {
            ATL_warnlog("Double dashes '-' or underscores '_' are not allowed");
            return false;
        }

        if ((c == DASH && prev == UNDERSCORE) || (c == UNDERSCORE && prev == DASH))
        {
            ATL_warnlog("Dash '-' followed by underscore '_' or vice versa are not allowed");
            return false;
        }
    }

    char last = str[len - 1];
    if (last == DASH || last == UNDERSCORE)
    {
        ATL_warnlog("Ending project name with dash '-' or underscore '_' are not allowed");
        return false;
    }

    return true;
}

bool atl_is_valid_file_name(const char *str)
{
    if (!str || !*str)
        return false;

    size_t len = strlen(str);
    if (len > (size_t) MAX_LEN)
    {
        ATL_warnlog("File name length %zu exceeds max allowed %d", len, MAX_LEN);
        return false;
    }

    // Special case: hidden files starting with '.'
    size_t start = 0;
    if (str[0] == '.')
    {
        start = 1;
        if (len == 1)
        {
            ATL_warnlog("Filename cannot be just '.'");
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
                ATL_warnlog("Multiple dots not allowed");
                return false;  // allow only one dot
            }
            if (i == start || i == len - 1)
            {
                ATL_warnlog("Dot cannot start or end the filename");
                return false;
            }
            dot_seen = true;
            continue;
        }

        if (!(isalnum((unsigned char) c) || c == DASH || c == UNDERSCORE))
        {
            ATL_warnlog("Invalid character: %c", c);
            return false;
        }

        // prevent consecutive special chars
        if ((c == DASH && prev == DASH) || (c == UNDERSCORE && prev == UNDERSCORE) ||
            (c == DASH && prev == UNDERSCORE) || (c == UNDERSCORE && prev == DASH))
        {
            ATL_warnlog("Consecutive or mixed '-' and '_' not allowed");
            return false;
        }
    }

    return true;
}
