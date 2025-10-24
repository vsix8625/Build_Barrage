#include "barr_package_scan_dir.h"
#include "barr_gc.h"
#include "barr_io.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static inline bool barr_is_pc_file(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext)
    {
        return false;
    }
    return BARR_strmatch(ext, ".pc");
}

static inline bool barr_skip_dir_pkg(const char *path)
{
    static const char *skip_names[] = {"build", "bin",        "obj",   ".git",    "cache", ".vs", ".idea",
                                       "test",  "CMakeFiles", "Debug", "Release", ".barr", NULL};

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

//----------------------------------------------------------------------------------------------------

static void barr_expand_pc_vars(const char *input, char *output, size_t out_size, const char *prefix,
                                const char *exec_prefix, const char *libdir, const char *sharedlibdir,
                                const char *includedir)
{
    output[0] = '\0';
    const char *p = input;
    while (*p)
    {
        const char *dollar = strstr(p, "${");
        if (!dollar)
        {
            strncat(output, p, out_size - strlen(output) - 1);
            break;
        }

        strncat(output, p, dollar - p);  // Copy up to ${
        const char *close = strchr(dollar, '}');
        if (!close)
        {
            strncat(output, dollar, out_size - strlen(output) - 1);
            break;
        }

        size_t varlen = close - (dollar + 2);
        if (strncmp(dollar + 2, "prefix", varlen) == 0)
        {
            strncat(output, prefix, out_size - strlen(output) - 1);
        }
        else if (strncmp(dollar + 2, "exec_prefix", varlen) == 0)
        {
            strncat(output, exec_prefix, out_size - strlen(output) - 1);
        }
        else if (strncmp(dollar + 2, "libdir", varlen) == 0)
        {
            strncat(output, libdir, out_size - strlen(output) - 1);
        }
        else if (strncmp(dollar + 2, "includedir", varlen) == 0)
        {
            strncat(output, includedir, out_size - strlen(output) - 1);
        }
        else if (strncmp(dollar + 2, "sharedlibdir", varlen) == 0)
        {
            strncat(output, sharedlibdir, out_size - strlen(output) - 1);
        }
        else
        {
            // Copy unknown variable as-is
            strncat(output, dollar, close - dollar + 1);
        }

        p = close + 1;
    }
}

static void barr_expand_var(const char *input, char *output, size_t out_size, const char *prefix,
                            const char *exec_prefix, const char *libdir, const char *sharedlibdir,
                            const char *includedir)
{
    barr_expand_pc_vars(input, output, out_size, prefix, exec_prefix, libdir, sharedlibdir, includedir);
}

static void barr_parse_pc_file(const char *filepath, BARR_PackageInfo *out)
{
    if (!filepath || !out)
    {
        return;
    }

    FILE *f = fopen(filepath, "r");
    if (!f)
    {
        BARR_errlog("%s(): failed to open .pc file: %s", __func__, filepath);
        return;
    }

    char line[BARR_BUF_SIZE_1024];
    char prefix[BARR_BUF_SIZE_256] = "";
    char exec_prefix[BARR_BUF_SIZE_256] = "";
    char libdir[BARR_BUF_SIZE_256] = "";
    char includedir[BARR_BUF_SIZE_256] = "";
    char sharedlibdir[BARR_BUF_SIZE_256] = "";
    char temp[BARR_BUF_SIZE_256];

    while (fgets(line, sizeof(line), f))
    {
        char *nl = strchr(line, '\n');
        if (nl)
        {
            *nl = '\0';
        }

        if (!line[0] || line[0] == '#')
        {
            continue;
        }

        char *equal = strchr(line, '=');
        if (equal)
        {
            *equal = '\0';
            char *key = line;
            char *val = equal + 1;

            if (BARR_strmatch(key, "prefix"))
            {
                strncpy(prefix, val, sizeof(prefix) - 1);
                prefix[sizeof(prefix) - 1] = '\0';
            }
            else if (BARR_strmatch(key, "exec_prefix"))
            {
                barr_expand_var(val, temp, sizeof(temp), prefix, exec_prefix, libdir, sharedlibdir, includedir);
                strncpy(exec_prefix, temp, sizeof(exec_prefix) - 1);
                exec_prefix[sizeof(exec_prefix) - 1] = '\0';
            }
            else if (BARR_strmatch(key, "libdir"))
            {
                barr_expand_var(val, temp, sizeof(temp), prefix, exec_prefix, libdir, sharedlibdir, includedir);
                strncpy(libdir, temp, sizeof(libdir) - 1);
                libdir[sizeof(libdir) - 1] = '\0';
            }
            else if (BARR_strmatch(key, "includedir"))
            {
                barr_expand_var(val, temp, sizeof(temp), prefix, exec_prefix, libdir, sharedlibdir, includedir);
                strncpy(includedir, temp, sizeof(includedir) - 1);
                includedir[sizeof(includedir) - 1] = '\0';
            }
            else if (BARR_strmatch(key, "sharedlibdir"))
            {
                barr_expand_var(val, temp, sizeof(temp), prefix, exec_prefix, libdir, sharedlibdir, includedir);
                strncpy(sharedlibdir, temp, sizeof(sharedlibdir) - 1);
                sharedlibdir[sizeof(sharedlibdir) - 1] = '\0';
            }
            continue;
        }

        char *colon = strchr(line, ':');
        if (!colon)
        {
            continue;
        }

        *colon = '\0';
        char *field = line;
        char *value = colon + 1;
        while (*value == ' ' || *value == '\t')
        {
            value++;
        }

        char buf[BARR_BUF_SIZE_1024];
        barr_expand_pc_vars(value, buf, sizeof(buf), prefix, exec_prefix, libdir, sharedlibdir, includedir);

        if (BARR_strmatch(field, "Name"))
        {
            out->name = BARR_gc_strdup(buf);
        }
        else if (BARR_strmatch(field, "Version"))
        {
            out->version = BARR_gc_strdup(buf);
        }
        else if (BARR_strmatch(field, "Libs"))
        {
            out->libs = BARR_gc_strdup(buf);
        }
        else if (BARR_strmatch(field, "Libs.private"))
        {
            out->libs_static = BARR_gc_strdup(buf);
        }
        else if (BARR_strmatch(field, "Cflags"))
        {
            out->cflags = BARR_gc_strdup(buf);
        }
    }

    out->libdir = BARR_gc_strdup(libdir);
    out->sharedlibdir = BARR_gc_strdup(sharedlibdir);
    out->includedir = BARR_gc_strdup(includedir);

    fclose(f);
}

//----------------------------------------------------------------------------------------------------

void BARR_package_scan_dir(BARR_PackageInfo *out, const char *dirpath, const char *target_pkg)
{
    if (!dirpath || !out)
    {
        return;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    size_t cap = BARR_BUF_SIZE_64;
    char **queue = BARR_gc_alloc(cap * sizeof(char *));
    size_t que_size = 0;
    size_t que_cap = cap;
    queue[que_size++] = BARR_gc_strdup(dirpath);

    while (que_size)
    {
        char *current = queue[--que_size];
        DIR *dir = opendir(current);
        if (!dir)
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

            char fullpath[BARR_BUF_SIZE_1024];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", current, dent->d_name);

            if (dent->d_type == DT_DIR)
            {
                if (!barr_skip_dir_pkg(dent->d_name))
                {
                    if (que_size >= que_cap)
                    {
                        que_cap *= 2;
                        char **new_que = BARR_gc_realloc(queue, que_cap * sizeof(char *));
                        if (!new_que)
                        {
                            BARR_errlog("%s(): failed to realloc", __func__);
                            break;
                        }
                        queue = new_que;
                    }
                    queue[que_size++] = BARR_gc_strdup(fullpath);
                }
            }
            else if (dent->d_type == DT_REG && barr_is_pc_file(fullpath))
            {
                BARR_PackageInfo temp;
                barr_parse_pc_file(fullpath, &temp);

                if (!target_pkg || BARR_strmatch(temp.name, target_pkg))
                {
                    *out = temp;
                    out->pkg_path = BARR_gc_strdup(current);
                    break;
                }
            }
        }

        closedir(dir);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (double) (end.tv_sec - start.tv_sec) + (double) (end.tv_nsec - start.tv_nsec) / 1e9;
    BARR_printf("Package scan done in %.6f sec\n", elapsed);
}
