#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include "barr_src_scan.h"
#include "barr_debug.h"
#include "barr_io.h"
#include "barr_sha256.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static inline barr_i32 barr_is_source_file(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext)
    {
        return 0;
    }

    return BARR_strmatch(ext, ".c") || BARR_strmatch(ext, ".cpp");
}

static inline bool barr_skip_dir(const char *path)
{
    static const char *skip_names[] = {"build", "bin",  "obj",        ".git",  "cache",   ".vs",
                                       ".idea", "test", "CMakeFiles", "Debug", "Release", NULL};

    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;

    for (const char **p = skip_names; *p; ++p)
    {
        if (BARR_strmatch(base, *p))
        {
            return true;
        }
    }
    return false;
}

void BARR_source_list_scan_dir(BARR_SourceList *list, const char *dirpath)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    //----------------------------------------------------------------------------------------------------

    if (!list || !dirpath)
    {
        BARR_errlog("%s(): invalid args", __func__);
        return;
    }

    char **queue = malloc(64 * sizeof(char *));
    if (!queue)
    {
        BARR_errlog("%s(): failed to allocate memory for queue", __func__);
        return;
    }

    size_t files_count = 0;
    size_t dir_count = 0;

    size_t que_size = 0;
    size_t que_cap = BARR_SCAN_QUEUE_CAP;

    queue[que_size++] = strdup(dirpath);

    for (size_t i = 0; i < que_size; ++i)
    {
        char *current = queue[i];
        dir_count++;
        DIR *dir = opendir(current);
        if (!dir)
        {
            free(current);
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

            char fullpath[BARR_PATH_MAX];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", current, dent->d_name);

            struct barr_stat st;

            if (dent->d_type == DT_DIR)
            {
                if (!barr_skip_dir(dent->d_name) && que_size >= que_cap)
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
    BARR_log("Found: %zu source files", list->count);
}
