#include "barr_batch_build.h"
#include "barr_debug.h"
#include "barr_io.h"
#include "barr_src_list.h"
#include <string.h>
#include <sys/resource.h>

static bool barr_is_merge_safe(const char *filepath, size_t max_lines)
{
    FILE *fp = fopen(filepath, "r");
    if (!fp)
    {
        return false;
    }

    char line[BARR_BUF_SIZE_512];
    size_t line_count = 0;
    bool safe = true;

    while (fgets(line, sizeof(line), fp))
    {
        line_count++;
        if (line_count > max_lines)
        {
            safe = false;
            break;
        }

        if (strstr(line, "static"))
        {
            safe = false;
            break;
        }

        if (strstr(line, "extern"))
        {
            safe = false;
            break;
        }

        if (strstr(line, "main("))  // never merge main files
        {
            safe = false;
            break;
        }
    }

    fclose(fp);
    return safe;
}

void BARR_create_batches(const BARR_SourceList *sources, BARR_SourceList *batch_list)
{
    if (sources == NULL || batch_list == NULL)
    {
        BARR_errlog("%s(): null list provided", __func__);
        return;
    }

    const size_t max_batch_size = BARR_BUF_SIZE_1024 * BARR_BUF_SIZE_1024 * 10;  // 10MB
    const size_t max_file_lines = 10000;
    const size_t max_file_per_batch = 50;

    barr_u32 current_batch_count = 0;
    FILE *batch_fp = NULL;
    char batch_path[BARR_PATH_MAX];

    for (size_t i = 0; i < sources->count; i++)
    {
        const char *src = sources->entries[i];
        if (src == NULL)
        {
            continue;
        }

        if (!barr_is_merge_safe(src, max_file_lines))
        {
            BARR_source_list_push(batch_list, src);
            continue;
        }

        if (!batch_fp)
        {
            if (!BARR_isdir("build"))
            {
                barr_mkdir("build");
            }
            barr_mkdir("build/batch");
            barr_mkdir("build/obj");
            snprintf(batch_path, sizeof(batch_path), "build/batch/tmp_batch_%u.c", current_batch_count++);
            batch_fp = fopen(batch_path, "w");
            if (!batch_fp)
            {
                BARR_errlog("%s(): failed to create batch file", __func__);
                continue;
            }
        }

        FILE *fp = fopen(src, "r");
        if (!fp)
        {
            BARR_warnlog("%s(): cannot open %s", __func__, src);
            continue;
        }

        fprintf(batch_fp, "// BEGIN FILE: %s\n", src);

        char buf[BARR_BUF_SIZE_4096];
        size_t total_bytes = 0;

        while (fgets(buf, sizeof(buf), fp))
        {
            (void) fputs(buf, batch_fp);
            total_bytes += strlen(buf);
            if (total_bytes > max_batch_size)
            {
                fclose(fp);
                fclose(batch_fp);
                batch_fp = NULL;
                BARR_source_list_push(batch_list, batch_path);
                goto next_batch;
            }
        }

        fclose(fp);
        fprintf(batch_fp, "\n // END FILE: %s\n\n", src);

    next_batch:
        if ((i % max_file_per_batch) == 0 && batch_fp)
        {
            fclose(batch_fp);
            batch_fp = NULL;
            BARR_source_list_push(batch_list, batch_path);
        }
    }

    if (batch_fp)
    {
        fclose(batch_fp);
        BARR_source_list_push(batch_list, batch_path);
    }

    BARR_dbglog("%s(): initial batch list built with %zu files", __func__, batch_list->count);
}
