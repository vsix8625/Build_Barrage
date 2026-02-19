#include "barr_batch_build.h"
#include "barr_cpu.h"
#include "barr_debug.h"
#include "barr_hashmap.h"
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

    char   line[BARR_BUF_SIZE_512];
    size_t line_count = 0;
    bool   safe       = true;

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

void BARR_create_batches(const BARR_SourceList *sources,
                         BARR_SourceList       *batch_list,
                         const char            *out_dir)
{
    if (sources == NULL || batch_list == NULL || out_dir == NULL)
    {
        BARR_errlog("%s(): null list provided", __func__);
        return;
    }

    if (!BARR_isdir(out_dir))
    {
        BARR_mkdir_p(out_dir);
    }

    // perf
    BARR_InfoCPU cpu = {0};
    BARR_get_cpu_info(&cpu);

    size_t max_batch_size = cpu.cache_size << 1;
    if (max_batch_size < (1UL << 20))
    {
        max_batch_size = 1UL << 20;
    }
    if (max_batch_size > (128UL << 20))
    {
        max_batch_size = 128UL << 20;
    }

    size_t avg_line_bytes = 128;
    size_t max_file_lines = max_batch_size / avg_line_bytes;
    if (max_file_lines < 20000)
    {
        max_file_lines = 20000;
    }
    if (max_file_lines > 200000)
    {
        max_file_lines = 200000;
    }

    const size_t max_file_per_batch =
        (size_t) (cpu.threads * 1024) <= 1024 * 12 ? (size_t) (cpu.threads * 1024) : 1024;

    size_t   files_in_current_batch = 0;
    barr_u32 current_batch_count    = 0;

    FILE *batch_fp = NULL;
    char  batch_path[BARR_PATH_MAX + BARR_PATH_MAX];

    BARR_HashMap *includes_map = BARR_hashmap_create(BARR_BUF_SIZE_128);

    bool skip_batch = false;
    // bool force_batch = false; into the void

    size_t batch_bytes = 0;

    for (size_t i = 0; i < sources->count; i++)
    {
        const char *src = sources->entries[i];
        if (src == NULL)
        {
            continue;
        }

        // Open file to check for skip comment
        FILE *fp_check = fopen(src, "r");
        if (fp_check)
        {
            char line[BARR_BUF_SIZE_512];

            for (size_t l = 0; l < 10 && fgets(line, sizeof(line), fp_check); l++)
            {
                if (strstr(line, "// barr-batch-skip") || strstr(line, "// barr-no-batch"))
                {
                    skip_batch = true;
                    break;
                }

                /* skipped for now or ever
                if (strstr(line, "// barr-batch-force"))
                {
                    force_batch = true;
                }
                */
            }
            fclose(fp_check);

            if (skip_batch)
            {
                if (batch_fp)
                {
                    BARR_log("[turbo]: Skipping %s — marked with '// barr-batch-skip'", src);
                    fclose(batch_fp);
                    BARR_source_list_push(batch_list, batch_path);
                    batch_fp               = NULL;
                    files_in_current_batch = 0;

                    BARR_destroy_hashmap(includes_map);
                    includes_map = NULL;
                }

                // push file directly without batching
                BARR_source_list_push(batch_list, src);
                continue;
            }
        }

        // skip for large files
        if (!barr_is_merge_safe(src, max_file_lines))
        {
            if (batch_fp)
            {
                fclose(batch_fp);
                BARR_source_list_push(batch_list, batch_path);
                batch_fp               = NULL;
                files_in_current_batch = 0;

                BARR_destroy_hashmap(includes_map);
                includes_map = NULL;
            }

            BARR_source_list_push(batch_list, src);
            continue;
        }

        if (!batch_fp)
        {
            snprintf(
                batch_path, sizeof(batch_path), "%s/batch_%u.c", out_dir, current_batch_count++);
            batch_fp = fopen(batch_path, "w");
            if (!batch_fp)
            {
                BARR_errlog("%s(): failed to create batch file", __func__);
                continue;
            }

            batch_bytes = 0;

            // reset includes for new batch
            if (includes_map)
            {
                BARR_destroy_hashmap(includes_map);
            }
            includes_map           = BARR_hashmap_create(BARR_BUF_SIZE_128);
            files_in_current_batch = 0;

            // write deduplicated includes at top (initially empty)
            for (size_t n = 0; n < includes_map->capacity; n++)
            {
                for (BARR_HashNode *node = includes_map->nodes[n]; node; node = node->next)
                {
                    fprintf(batch_fp, "%s\n", node->key);
                }
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

        while (fgets(buf, sizeof(buf), fp))
        {
            if (strncmp(buf, "#include", 8) == 0)
            {
                size_t len = strlen(buf);
                if (len && buf[len - 1] == '\n')
                {
                    buf[len - 1] = '\0';
                }

                if (!BARR_hashmap_get(includes_map, buf))
                {
                    barr_u8 dummy_hash[BARR_XXHASH_LEN] = {0};
                    BARR_hashmap_insert(includes_map, buf, dummy_hash);

                    fprintf(batch_fp, "%s\n", buf);
                }
                continue;
            }

            fputs(buf, batch_fp);
            size_t len   = strlen(buf);
            batch_bytes += len;

            if (batch_bytes > max_batch_size)
            {
                fclose(fp);

                // prepend deduped includes
                for (size_t n = 0; n < includes_map->capacity; n++)
                {
                    for (BARR_HashNode *node = includes_map->nodes[n]; node; node = node->next)
                    {
                        fprintf(batch_fp, "%s\n", node->key);
                    }
                }

                fclose(batch_fp);
                BARR_source_list_push(batch_list, batch_path);
                batch_fp               = NULL;
                files_in_current_batch = 0;
                batch_bytes            = 0;
                goto next_src_file;
            }
        }

        fclose(fp);
        fprintf(batch_fp, "\n // END FILE: %s\n\n", src);
        files_in_current_batch++;

        if (files_in_current_batch >= max_file_per_batch)
        {
            fclose(batch_fp);
            BARR_source_list_push(batch_list, batch_path);
            batch_fp               = NULL;
            files_in_current_batch = 0;

            // reset includes for next batch
            BARR_destroy_hashmap(includes_map);
            includes_map = BARR_hashmap_create(BARR_BUF_SIZE_128);
        }

    next_src_file:;
    }

    if (batch_fp)
    {
        fclose(batch_fp);
        BARR_source_list_push(batch_list, batch_path);

        BARR_destroy_hashmap(includes_map);
    }
}
