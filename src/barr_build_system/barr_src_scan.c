#define _FILE_OFFSET_BITS 64

#include "barr_src_scan.h"
#include "barr_debug.h"
#include "barr_env.h"
#include "barr_gc.h"
#include "barr_io.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static inline barr_i32 barr_is_header_file(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (ext == NULL)
    {
        return 0;
    }

    return BARR_strmatch(ext, ".h") || BARR_strmatch(ext, ".hpp");
}

static inline barr_i32 barr_is_source_file(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (ext == NULL)
    {
        return 0;
    }

    return BARR_strmatch(ext, ".c") || BARR_strmatch(ext, ".cpp");
}

static inline bool barr_is_project_root(const char *dirpath)
{
    if (dirpath == NULL)
    {
        return false;
    }

    static const char *excludes[] = {"Barrfile",     "Makefile",   "pyproject.toml", "CMakeLists.txt", "setup.py",
                                     "package.json", "Cargo.toml", "meson.build",    "SConstruct",     "go.md",
                                     ".csproj",      "WORKSPACE",  "WORSPACE.baze",  "BUILD",          "BUILD.bazel",
                                     "configure.ac", NULL};

    char config_path[BARR_PATH_MAX + 32];
    for (const char **filename = excludes; *filename; ++filename)
    {
        snprintf(config_path, sizeof(config_path), "%s/%s", dirpath, *filename);
        if (BARR_isfile(config_path))
        {
            BARR_dbglog("Excluding directory '%s' contains '%s'", config_path, *filename);
            return true;
        }
    }

    return false;
}

static inline bool barr_exclude(const char *path, const char **exclude_patterns)
{
    static const char *skip_names[] = {
        "build",   "bin",     "obj",   ".git",   "cache",  ".vs",     ".idea",  "test",           "CMakeFiles",
        "Debug",   "Release", ".barr", "docs",   "assets", "scripts", "modes",  ".barr_recovery", "pycache",
        ".vscode", "objects", "out",   "target", ".svn",   ".hg",     "vendor", ".trash",         NULL};

    if (strstr(path, "/.") != NULL)
    {
        return true;
    }

    for (const char **p = skip_names; *p; ++p)
    {
        const char *found = strstr(path, *p);
        if (found)
        {
            /* make sure it's a directory component, not substring of another name */
            char before = (found == path) ? '/' : *(found - 1);
            char after = *(found + strlen(*p));
            if ((before == '/' || before == '\0') && (after == '/' || after == '\0'))
            {
                return true;
            }
        }
    }

    if (exclude_patterns)
    {
        for (const char **p = exclude_patterns; *p; ++p)
        {
            const char *found = strstr(path, *p);
            if (found)
            {
                char before = (found == path) ? '/' : *(found - 1);
                char after = *(found + strlen(*p));
                if ((before == '/' || before == '\0') && (after == '/' || after == '\0'))
                {
                    return true;
                }
            }
        }
    }

    /* last segment guard */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;

    if (base[0] == '.')
    {
        return true;
    }

    return false;
}

void BARR_source_list_scan_dir(BARR_SourceList *list, const char *dirpath, const char **exclude_tokens)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    //----------------------------------------------------------------------------------------------------

    if (list == NULL || dirpath == NULL)
    {
        BARR_errlog("%s(): invalid args", __func__);
        return;
    }

    char **queue = malloc(64 * sizeof(char *));
    if (queue == NULL)
    {
        BARR_errlog("%s(): failed to allocate memory for queue", __func__);
        return;
    }

    size_t files_count = 0;
    size_t dir_count = 0;

    size_t que_size = 0;
    size_t que_cap = BARR_SCAN_QUEUE_CAP;

    queue[que_size++] = strdup(dirpath);
    if (queue[0] == NULL)
    {
        BARR_errlog("%s(): failed to strdup initial dirpath", __func__);
        free(queue);
        return;
    }

    for (size_t i = 0; i < que_size; ++i)
    {
        char *current = queue[i];
        if (current == NULL)
        {
            BARR_warnlog("%s(): skipping null path in queue", __func__);
            continue;
        }

        dir_count++;
        DIR *dir = opendir(current);
        if (dir == NULL)
        {
            BARR_warnlog("%s(): cannot open dir '%s'", __func__, current);
            continue;
        }

        barr_i32 fd = dirfd(dir);
        if (fd >= 0)
        {
            readahead(fd, 0, BARR_SCAN_READAHEAD_SIZE);
        }

        struct dirent *dent;
        while ((dent = readdir(dir)))
        {
            if (BARR_strmatch(dent->d_name, ".") || BARR_strmatch(dent->d_name, ".."))
            {
                continue;
            }

            if (barr_exclude(dent->d_name, exclude_tokens))
            {
                continue;
            }

            char *fullpath = BARR_gc_alloc(BARR_PATH_MAX);
            snprintf(fullpath, BARR_PATH_MAX, "%s/%s", current, dent->d_name);

            struct barr_stat st;

            if (dent->d_type == DT_DIR)
            {
                if (barr_is_project_root(fullpath))
                {
                    BARR_dbglog("%s(): skipped '%s'", __func__, fullpath);
                    continue;
                }

                if (que_size >= que_cap)
                {
                    que_cap *= 2;
                    char **new_queue = realloc(queue, que_cap * sizeof(char *));
                    if (!new_queue)
                    {
                        BARR_errlog("%s(): failed to realloc", __func__);
                        closedir(dir);

                        for (size_t j = 0; j < que_size; ++j)
                        {
                            free(queue[j]);
                        }
                        free(queue);
                        return;
                    }
                    queue = new_queue;
                }
                queue[que_size++] = strdup(fullpath);
            }
            else
            {
                files_count++;

                if ((dent->d_type == DT_REG && barr_is_source_file(fullpath)) ||
                    (dent->d_type != DT_REG && barr_stat(fullpath, &st) == 0 && S_ISREG(st.st_mode) &&
                     barr_is_source_file(fullpath)))
                {
                    if (!BARR_source_list_push(list, fullpath))
                    {
                        BARR_warnlog("%s(): failed to push to list", __func__);
                        for (size_t j = 0; j < que_size; ++j)
                        {
                            free(queue[j]);
                        }
                        free(queue);
                        closedir(dir);
                        return;
                    }
                }
            }
        }

        closedir(dir);

        //----------------------------------------------------------------------------------------------------
        // CLOCK
        clock_gettime(CLOCK_MONOTONIC, &end);
        barr_i64 sec = (barr_i64) (end.tv_sec - start.tv_sec);
        barr_i64 nsec = (barr_i64) (end.tv_nsec - start.tv_nsec);
        if (nsec < 0)
        {
            --sec;
            nsec += 1000000000LL;
        }
        double elapsed = (double) sec + (double) nsec / 1e9;
        BARR_printf("\rScanned: %zu files in %zu dirs (%.2f files per sec) in \033[34;1m%.6fsec", files_count,
                    dir_count, (double) list->count / elapsed, elapsed);
    }
    for (size_t j = 0; j < que_size; ++j)
    {
        free(queue[j]);
    }
    free(queue);
    printf("\n");
    if (g_barr_verbose)
    {
        BARR_log("Found: %zu source files", list->count);
    }
}

void BARR_header_list_scan_dir(BARR_SourceList *list, const char *dirpath, BARR_SourceList *inc_dir_list,
                               const char **exclude_tokens)
{
    if (list == NULL || dirpath == NULL)
    {
        BARR_errlog("%s(): invalid args", __func__);
        return;
    }

    char **queue = BARR_gc_alloc(64 * sizeof(char *));
    if (queue == NULL)
    {
        BARR_errlog("%s(): failed to allocate memory for queue", __func__);
        return;
    }

    size_t que_size = 0;
    size_t que_cap = BARR_SCAN_QUEUE_CAP;

    queue[que_size++] = BARR_gc_strdup(dirpath);
    if (queue[0] == NULL)
    {
        BARR_errlog("%s(): failed to strdup initial dirpath", __func__);
        free(queue);
        return;
    }

    for (size_t i = 0; i < que_size; ++i)
    {
        char *current = queue[i];
        DIR *dir = opendir(current);
        if (dir == NULL)
        {
            continue;
        }

        struct dirent *dent;
        while ((dent = readdir(dir)))
        {
            if (BARR_strmatch(dent->d_name, ".") || BARR_strmatch(dent->d_name, ".."))
            {
                continue;
            }

            char fullpath[BARR_PATH_MAX];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", current, dent->d_name);

            if (dent->d_type == DT_DIR)
            {
                if (!barr_exclude(dent->d_name, exclude_tokens))
                {
                    if (barr_is_project_root(fullpath))
                    {
                        BARR_dbglog("%s(): skipped '%s'", __func__, fullpath);
                        continue;
                    }

                    if (inc_dir_list)
                    {
                        BARR_source_list_push(inc_dir_list, fullpath);
                    }
                    if (que_size >= que_cap)
                    {
                        que_cap *= 2;
                        char **new_queue = BARR_gc_realloc(queue, que_cap * sizeof(char *));
                        if (!new_queue)
                        {
                            BARR_errlog("%s(): failed to realloc", __func__);
                            closedir(dir);
                        }
                        queue = new_queue;
                    }
                    queue[que_size++] = BARR_gc_strdup(fullpath);
                }
            }
            else if (dent->d_type == DT_REG && barr_is_header_file(fullpath))
            {
                BARR_source_list_push(list, fullpath);
            }
        }
        closedir(dir);
    }

    if (g_barr_verbose)
    {
        BARR_log("Found: %zu header files", list->count);
    }
}
