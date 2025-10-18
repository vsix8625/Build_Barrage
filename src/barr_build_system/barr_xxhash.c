#include "barr_xxhash.h"
#include "barr_hashmap.h"
#include "barr_io.h"

#include <ctype.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xxhash.h>

// ----------------------------------------------------------------------------------------------------

static bool barr_filestack_push(BARR_Filestack *fstack, const char *file)
{
    if (!fstack)
    {
        return false;
    }
    if (fstack->count >= fstack->capacity)
    {
        size_t new_cap = fstack->capacity * 2;
        char **tmp = realloc(fstack->files, new_cap * sizeof(char *));
        if (!tmp)
        {
            return false;
        }

        fstack->files = tmp;
        fstack->capacity = new_cap;
    }
    fstack->files[fstack->count++] = strdup(file);
    return true;
}

static char *BARR_filestack_pop(BARR_Filestack *fstack)
{
    if (!fstack || fstack->count == 0)
    {
        return NULL;
    }
    return fstack->files[--fstack->count];
}

static void barr_filestack_free(BARR_Filestack *fstack)
{
    if (!fstack)
    {
        return;
    }
    for (size_t i = 0; i < fstack->count; i++)
    {
        if (fstack->files[i])
        {
            free(fstack->files[i]);
        }
    }
    free(fstack->files);
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

bool BARR_hash_includes_xxh3(const char *filepath, barr_u8 out_hash[BARR_XXHASH_LEN])
{
    BARR_HashMap *seen = BARR_hashmap_create(BARR_BUF_SIZE_4096);
    if (!seen)
    {
        BARR_errlog("%s(): failed to create hashmap for filestack", __func__);
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

    if (!barr_filestack_push(&fstack, filepath))
    {
        barr_filestack_free(&fstack);
        BARR_destroy_hashmap(seen);
        BARR_errlog("%s(): failed to push file '%s' to hashmap", __func__, filepath);
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

        FILE *fp = fopen(filepath, "r");
        if (!fp)
        {
            BARR_warnlog("%s(): failed to open \"%s\"", __func__, current_file);
            free(current_file);
            continue;
        }

        if (fseek(fp, 0, SEEK_END) == 0)
        {
            barr_i64 fsize = ftell(fp);
            if (fseek(fp, 0, SEEK_SET) == 0)
            {
                char *buffer = malloc(fsize);
                if (buffer)
                {
                    size_t bytes_read = fread(buffer, 1, fsize, fp);
                    if (bytes_read > 0)
                    {
                        XXH3_64bits_update(state, buffer, fsize);
                        free(buffer);
                    }
                }
                fclose(fp);
            }
        }

        fp = fopen(current_file, "r");
        if (fp)
        {
            char line[BARR_BUF_SIZE_1024];
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
                            barr_filestack_push(&fstack, full_include_path);
                        }
                    }
                }
            }
        }
        fclose(fp);
    }

    free(current_file);

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
