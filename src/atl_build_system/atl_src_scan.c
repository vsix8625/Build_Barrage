#include "atl_src_scan.h"
#include "atl_debug.h"
#include "atl_io.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline atl_i32 atl_is_source_file(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext)
    {
        return 0;
    }

    return ATL_strmatch(ext, ".c") || ATL_strmatch(ext, ".cpp");
}

static inline bool atl_skip_dir(const char *path)
{
    static const char *skip_names[] = {"build", "bin",  "obj",        ".git",  "cache",   ".vs",
                                       ".idea", "test", "CMakeFiles", "Debug", "Release", NULL};

    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;

    for (const char **p = skip_names; *p; ++p)
    {
        if (ATL_strmatch(base, *p))
        {
            return true;
        }
    }
    return false;
}

void ATL_source_list_scan_dir(ATL_SourceList *list, const char *dirpath)
{
    if (!list || !dirpath)
    {
        ATL_errlog("%s(): Invalid args", __func__);
        return;
    }

    char **stack = NULL;
    size_t stack_size = 0;
    size_t stack_cap = 64;
    stack = malloc(stack_cap * sizeof(char *));
    if (!stack)
    {
        ATL_errlog("%s(): Failed to allocate memory", __func__);
        return;
    }

    stack[stack_size++] = strdup(dirpath);

    while (stack_size)
    {
        char *path = stack[--stack_size];
        DIR *dir = opendir(path);
        if (!dir)
        {
            free(path);
            continue;
        }

        struct dirent *dent;
        while ((dent = readdir(dir)))
        {
            if (ATL_strmatch(dent->d_name, ".") || ATL_strmatch(dent->d_name, ".."))
            {
                continue;
            }

            char fullpath[ATL_PATH_MAX];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, dent->d_name);

            struct atl_stat st;
            if (atl_stat(fullpath, &st) != 0)
            {
                continue;
            }

            if (S_ISDIR(st.st_mode))
            {
                if (atl_skip_dir(dent->d_name))
                {
                    continue;
                }

                if (stack_size >= stack_cap)
                {
                    stack_cap *= 2;
                    char **new_stack = realloc(stack, stack_cap * sizeof(char *));
                    if (!new_stack)
                    {
                        ATL_errlog("%s(): Failed to realloc", __func__);
                        closedir(dir);
                        free(path);
                        free(stack);
                        return;
                    }
                    stack = new_stack;
                }

                stack[stack_size++] = strdup(fullpath);
            }
            else if (S_ISREG(st.st_mode))
            {
                if (atl_is_source_file(fullpath))
                {
                    if (!ATL_source_list_push(list, fullpath))
                    {
                        ATL_errlog("%s(): failed to push to source list", __func__);
                        closedir(dir);
                        free(path);
                        free(stack);
                        return;
                    }
                }
            }
        }

        closedir(dir);
        free(path);
    }

    free(stack);
}
