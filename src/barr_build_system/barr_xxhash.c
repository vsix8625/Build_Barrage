#include "barr_xxhash.h"
#include "barr_debug.h"
#include "barr_hashmap.h"
#include "barr_io.h"

#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xxhash.h>

// ----------------------------------------------------------------------------------------------------

bool BARR_resolve_path(const char *base_dir, const char *input_path, char out_path[BARR_PATH_MAX])
{
    if (!input_path || !out_path)
    {
        return false;
    }

    char tmp_path[BARR_PATH_MAX + 2];
    if (input_path[0] == '/')
    {
        strncpy(tmp_path, input_path, sizeof(tmp_path));
        tmp_path[sizeof(tmp_path) - 1] = '\0';
    }
    else
    {
        char base[BARR_PATH_MAX];
        if (base_dir)
        {
            strncpy(base, base_dir, sizeof(base));
            base[sizeof(base) - 1] = '\0';
        }
        else
        {
            if (!barr_getcwd(base, sizeof(base)))
            {
                BARR_errlog("%s(): getcwd failed", __func__);
                return false;
            }
        }

        snprintf(tmp_path, sizeof(tmp_path), "%s/%s", base, input_path);
    }

    if (realpath(tmp_path, out_path) == NULL)
    {
        char tmp[BARR_PATH_MAX];
        strncpy(tmp, tmp_path, sizeof(tmp));
        tmp[sizeof(tmp) - 1] = '\0';

        char *segments[BARR_BUF_SIZE_256];
        size_t seg_count = 0;

        char *token = strtok(tmp, "/");
        while (token && seg_count < (sizeof(segments) / sizeof(segments[0])))
        {
            if (BARR_strmatch(token, "."))
            {
                continue;
            }
            else if (BARR_strmatch(token, ".."))
            {
                if (seg_count > 0)
                {
                    seg_count--;
                }
            }
            else
            {
                segments[seg_count++] = token;
            }
            token = strtok(NULL, "/");
        }

        out_path[0] = '/';
        out_path[1] = '\0';

        for (size_t i = 0; i < seg_count; i++)
        {
            strncat(out_path, segments[i], BARR_PATH_MAX - strlen(out_path) - 1);
            if (i + 1 < seg_count)
            {
                strncat(out_path, "/", BARR_PATH_MAX - strlen(out_path) - 1);
            }
        }
    }

    return true;
}

// ----------------------------------------------------------------------------------------------------

static bool barr_filestack_push(BARR_Filestack *flstack, const char *file)
{
    if (!flstack)
    {
        return false;
    }
    if (flstack->count >= flstack->capacity)
    {
        size_t new_cap = flstack->capacity * 2;
        char **tmp = realloc(flstack->files, new_cap * sizeof(char *));
        if (!tmp)
        {
            return false;
        }

        flstack->files = tmp;
        flstack->capacity = new_cap;
    }
    flstack->files[flstack->count++] = strdup(file);
    return true;
}

static char *BARR_filestack_pop(BARR_Filestack *flstack)
{
    if (!flstack || flstack->count == 0)
    {
        return NULL;
    }
    return flstack->files[--flstack->count];
}

static void barr_filestack_free(BARR_Filestack *flstack)
{
    if (!flstack)
    {
        return;
    }

    if (flstack->count > 0)
    {
        BARR_dbglog("%s(): freeing %zu filestack entries", __func__, flstack->count);
    }
    for (size_t i = 0; i < flstack->count; i++)
    {
        if (flstack->files[i])
        {
            free(flstack->files[i]);
        }
    }
    free(flstack->files);
}

// ----------------------------------------------------------------------------------------------------

bool BARR_hash_file_xxh3(const char *filepath, barr_u8 out_hash[BARR_XXHASH_LEN])
{
    FILE *fp = fopen(filepath, "rb");
    if (!fp)
    {
        BARR_errlog("%s(): failed to open \"%s\" to read", __func__, filepath);
        return false;
    }

    XXH3_state_t *state = XXH3_createState();
    if (!state)
    {
        fclose(fp);
        BARR_errlog("%s(): failed to allocate XXH3 state", __func__);
        return false;
    }

    if (XXH3_64bits_reset(state) == XXH_ERROR)
    {
        BARR_errlog("%s(): failed to initialize XXH3", __func__);
        XXH3_freeState(state);
        fclose(fp);
        return false;
    }

    barr_u8 buf[16 * BARR_BUF_SIZE_1024];
    size_t n;
    bool success = true;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
    {
        if (XXH3_64bits_update(state, buf, n) == XXH_ERROR)
        {
            BARR_errlog("%s(): failed to update XXH3 with file data", __func__);
            success = false;
            break;
        }
    }

    if (!success || ferror(fp))
    {
        if (ferror(fp))
        {
            BARR_errlog("%s(): error reading file \"%s\"", __func__, filepath);
        }
        XXH3_freeState(state);
        fclose(fp);
        return false;
    }

    fclose(fp);

    XXH64_hash_t hash = XXH3_64bits_digest(state);
    XXH3_freeState(state);
    memcpy(out_hash, &hash, BARR_XXHASH_LEN);

    return success;
}

bool BARR_hash_includes_xxh3(const char *filapath, barr_u8 out_hash[BARR_XXHASH_LEN])
{
    BARR_HashMap *seen = BARR_hashmap_create(BARR_BUF_SIZE_4096);
    if (!seen)
    {
        BARR_errlog("%s(): failed to create hashmap for filestack", __func__);
        return false;
    }

    char resolved[BARR_PATH_MAX];
    if (!BARR_resolve_path(NULL, filapath, resolved))
    {
        BARR_errlog("%s(): failed to resolve path: %s", __func__, filapath);
        return false;
    }

    BARR_Filestack fstack = {0};
    fstack.capacity = BARR_BUF_SIZE_64;
    fstack.files = calloc(fstack.capacity, sizeof(char *));
    if (!fstack.files)
    {
        BARR_destroy_hashmap(seen);
        BARR_errlog("%s(): failed to allocate memory for filestack files", __func__);
        return false;
    }

    if (!barr_filestack_push(&fstack, resolved))
    {
        barr_filestack_free(&fstack);
        BARR_destroy_hashmap(seen);
        BARR_errlog("%s(): failed to push file '%s' to hashmap", __func__, resolved);
        return false;
    }

    XXH3_state_t *state = XXH3_createState();
    if (!state)
    {
        barr_filestack_free(&fstack);
        BARR_destroy_hashmap(seen);
        return false;
    }
    XXH3_64bits_reset(state);

    char *current_file;
    while ((current_file = BARR_filestack_pop(&fstack)) != NULL)
    {
        if (BARR_hashmap_get(seen, current_file))
        {
            free(current_file);
            continue;
        }

        barr_u8 file_hash[BARR_XXHASH_LEN];
        if (BARR_hash_file_xxh3(current_file, file_hash))
        {
            XXH3_64bits_update(state, file_hash, BARR_XXHASH_LEN);
        }
        else
        {
            BARR_warnlog("%s(): failed to hash %s", __func__, current_file);
        }

        barr_u8 dummy_hash[BARR_XXHASH_LEN] = {0};
        BARR_hashmap_insert(seen, current_file, dummy_hash);  // unused

        FILE *fp = fopen(current_file, "r");
        if (!fp)
        {
            BARR_warnlog("%s(): failed to open \"%s\"", __func__, current_file);
            free(current_file);
            continue;
        }

        if (fseek(fp, 0, SEEK_END) == 0)
        {
            barr_i64 ftell_result = ftell(fp);
            if (ftell_result == -1 || ftell_result < 0)
            {
                BARR_warnlog("%s(): ftell failed for %s: %s", __func__, current_file, strerror(errno));
                fclose(fp);
                free(current_file);
                continue;
            }
            size_t fsize = (size_t) ftell_result;
            if (fsize > BARR_MAX_FILE_SIZE)
            {
                BARR_warnlog("%s(): file %s too large (%zu bytes)", __func__, current_file, fsize);
                fclose(fp);
                free(current_file);
                continue;
            }
            if (fseek(fp, 0, SEEK_SET) != 0)
            {
                BARR_warnlog("%s(): fseek failed for %s: %s", __func__, current_file, strerror(errno));
                fclose(fp);
                free(current_file);
                continue;
            }
            char buffer[BARR_BUF_SIZE_16K * 2];
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0)
            {
                XXH3_64bits_update(state, buffer, bytes_read);
            }
            if (ferror(fp))
            {
                BARR_warnlog("%s(): error reading %s: %s", __func__, current_file, strerror(errno));
            }
            fclose(fp);
        }
        else
        {
            BARR_warnlog("%s(): fseek to end failed for %s: %s", __func__, current_file, strerror(errno));
            fclose(fp);
            free(current_file);
            continue;
        }

        fp = fopen(current_file, "r");
        if (fp)
        {
            char line[BARR_BUF_SIZE_2048];
            while (fgets(line, sizeof(line), fp))
            {
                char *p = line;
                while (isspace((barr_u8) *p))
                {
                    p++;
                }

                if (strncmp(p, "#include", 8) == 0)
                {
                    p += 8;
                    while (isspace((barr_u8) *p))
                    {
                        p++;
                    }

                    if (*p == '\"')
                    {
                        p++;
                        char include_file[BARR_BUF_SIZE_512];
                        barr_i32 i = 0;
                        while (*p && *p != '\"' && i < (barr_i32) (sizeof(include_file) - 1))
                        {
                            include_file[i++] = *p++;
                        }
                        include_file[i] = '\0';

                        char full_include_path[BARR_PATH_MAX];
                        char filepath_cpy[BARR_PATH_MAX];

                        strncpy(filepath_cpy, current_file, sizeof(filepath_cpy));
                        filepath_cpy[sizeof(filepath_cpy) - 1] = '\0';

                        char *dir = dirname(filepath_cpy);
                        snprintf(full_include_path, sizeof(full_include_path), "%s/%s", dir, include_file);

                        if (!BARR_hashmap_get(seen, full_include_path))
                        {
                            if (!barr_filestack_push(&fstack, full_include_path))
                            {
                                BARR_warnlog("%s(): failed to push include %s", __func__, full_include_path);
                                free(current_file);
                            }
                        }
                    }
                }
            }
            fclose(fp);
        }
        else
        {
            BARR_warnlog("%s(): failed to open %s for includes: %s", __func__, current_file, strerror(errno));
            free(current_file);
            continue;
        }
        free(current_file);
    }

    XXH64_hash_t final_hash = XXH3_64bits_digest(state);
    memcpy(out_hash, &final_hash, BARR_XXHASH_LEN);
    XXH3_freeState(state);
    barr_filestack_free(&fstack);
    BARR_destroy_hashmap(seen);

    return true;
}

bool BARR_hash_flags_xxh3(const char *flags, barr_u8 out_hash[BARR_XXHASH_LEN])
{
    XXH3_state_t *state = XXH3_createState();
    if (!state)
    {
        BARR_errlog("%s(): failed to allocate XXH3 state", __func__);
        return false;
    }

    if (XXH3_64bits_reset(state) == XXH_ERROR)
    {
        BARR_errlog("%s(): failed to initialize XXH3", __func__);
        XXH3_freeState(state);
        return false;
    }

    if (XXH3_64bits_update(state, flags, strlen(flags)) == XXH_ERROR)
    {
        BARR_errlog("%s(): failed to update XXH3 with flags", __func__);
        XXH3_freeState(state);
        return false;
    }

    XXH64_hash_t hash = XXH3_64bits_digest(state);
    XXH3_freeState(state);
    memcpy(out_hash, &hash, BARR_XXHASH_LEN);

    return true;
}

bool BARR_hashes_merge_xxh3(barr_u8 file_hash[BARR_XXHASH_LEN], barr_u8 include_hash[BARR_XXHASH_LEN],
                            barr_u8 flags_hash[BARR_XXHASH_LEN], barr_u8 out_hash[BARR_XXHASH_LEN])
{
    XXH3_state_t *state = XXH3_createState();
    if (!state)
    {
        BARR_errlog("%s(): failed to allocate XXH3 state", __func__);
        return false;
    }

    if (XXH3_64bits_reset(state) == XXH_ERROR)
    {
        BARR_errlog("%s(): failed to initialize XXH3", __func__);
        XXH3_freeState(state);
        return false;
    }

    // Update with all hashes
    if (XXH3_64bits_update(state, file_hash, BARR_XXHASH_LEN) == XXH_ERROR)
    {
        BARR_errlog("%s(): failed to update XXH3 with file_hash", __func__);
        XXH3_freeState(state);
        return false;
    }
    if (XXH3_64bits_update(state, include_hash, BARR_XXHASH_LEN) == XXH_ERROR)
    {
        BARR_errlog("%s(): failed to update XXH3 with include_hash", __func__);
        XXH3_freeState(state);
        return false;
    }
    if (XXH3_64bits_update(state, flags_hash, BARR_XXHASH_LEN) == XXH_ERROR)
    {
        BARR_errlog("%s(): failed to update XXH3 with flags_hash", __func__);
        XXH3_freeState(state);
        return false;
    }

    XXH64_hash_t hash = XXH3_64bits_digest(state);
    XXH3_freeState(state);
    memcpy(out_hash, &hash, BARR_XXHASH_LEN);

    return true;
}

// ----------------------------------------------------------------------------------------------------
