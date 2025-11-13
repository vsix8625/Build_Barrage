#include "barr_xxhash.h"
#include "barr_debug.h"
#include "barr_gc.h"
#include "barr_hashmap.h"
#include "barr_io.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xxhash.h>

// ----------------------------------------------------------------------------------------------------

static bool BARR_str_endswith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return false;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr)
        return false;
    return strcmp(str + lenstr - lensuffix, suffix) == 0;
}

static bool barr_resolve_include_from_list(const BARR_SourceList *headers, const char *current_file,
                                           const char *include_file, char out_path[BARR_PATH_MAX])
{
    if (!headers || !current_file || !include_file || !out_path)
    {
        return false;
    }

    char fullpath[BARR_PATH_MAX];
    char cwd_copy[BARR_PATH_MAX];

    strncpy(cwd_copy, current_file, sizeof(cwd_copy) - 1);
    cwd_copy[sizeof(cwd_copy) - 1] = '\0';
    char *dir = dirname(cwd_copy);

    snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, include_file);
    if (barr_access(fullpath, F_OK) == 0)
    {
        strlcpy(out_path, fullpath, BARR_PATH_MAX);
        return true;
    }

    for (size_t i = 0; i < headers->count; ++i)
    {
        if (BARR_str_endswith(headers->entries[i], include_file))
        {
            strncpy(out_path, headers->entries[i], BARR_PATH_MAX - 1);
            out_path[BARR_PATH_MAX - 1] = '\0';
            return true;
        }
    }

    return false;
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
    if (filepath == NULL)
    {
        BARR_errlog("%s(): filepath is NULL", __func__);
        return false;
    }

    FILE *fp = fopen(filepath, "rb");
    if (!fp)
    {
        BARR_errlog("%s(): failed to open file to read", __func__);
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

// TODO: take a deeper look on includes discovery
// NOTE: will need to create a bidirectional walker
// starts from file_path -> children and friends.
// if not match check parent and friends
// hard stop cwd()
bool BARR_hash_includes_xxh3(const BARR_SourceList *headers, const char *file_path, barr_u8 out_hash[BARR_XXHASH_LEN])
{
    if (!file_path || !headers || !out_hash)
    {
        return false;
    }

    BARR_HashMap *seen = BARR_hashmap_create(BARR_BUF_SIZE_4096);
    if (!seen)
    {
        BARR_errlog("%s(): failed to create hashmap for seen files", __func__);
        return false;
    }

    BARR_Filestack fstack = {0};
    fstack.capacity = BARR_BUF_SIZE_64;
    fstack.files = calloc(fstack.capacity, sizeof(char *));
    if (!fstack.files)
    {
        BARR_destroy_hashmap(seen);
        BARR_errlog("%s(): failed to allocate filestack", __func__);
        return false;
    }

    if (!barr_filestack_push(&fstack, file_path))
    {
        barr_filestack_free(&fstack);
        BARR_destroy_hashmap(seen);
        BARR_errlog("%s(): failed to push initial file '%s'", __func__, file_path);
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

        // Hash the file contents
        barr_u8 file_hash[BARR_XXHASH_LEN];
        if (BARR_hash_file_xxh3(current_file, file_hash))
        {
            XXH3_64bits_update(state, file_hash, BARR_XXHASH_LEN);
        }
        else
        {
            BARR_warnlog("%s(): failed to hash %s", __func__, current_file);
        }

        // Mark as seen
        barr_u8 dummy[BARR_XXHASH_LEN] = {0};
        BARR_hashmap_insert(seen, current_file, dummy);

        FILE *fp = fopen(current_file, "r");
        if (!fp)
        {
            BARR_warnlog("%s(): cannot open file %s: %s", __func__, current_file, strerror(errno));
            free(current_file);
            continue;
        }

        char line[BARR_BUF_SIZE_2048];
        while (fgets(line, sizeof(line), fp))
        {
            char *p = line;
            while (isspace((barr_u8) *p))
                p++;

            if (strncmp(p, "#include", 8) != 0)
                continue;
            p += 8;
            while (isspace((barr_u8) *p))
                p++;

            if (*p != '\"')
                continue;
            p++;

            char include_file[BARR_BUF_SIZE_512] = {0};
            int i = 0;
            while (*p && *p != '\"' && i < (int) (sizeof(include_file) - 1))
            {
                include_file[i++] = *p++;
            }
            include_file[i] = '\0';

            if (i == 0)
                continue;

            char resolved[BARR_PATH_MAX] = {0};
            if (!barr_resolve_include_from_list(headers, current_file, include_file, resolved))
            {
                BARR_warnlog("%s(): cannot resolve include %s in %s", __func__, include_file, current_file);
                continue;
            }

            if (!BARR_hashmap_get(seen, resolved))
            {
                if (!barr_filestack_push(&fstack, resolved))
                {
                    BARR_warnlog("%s(): failed to push resolved include %s", __func__, resolved);
                }
            }
        }
        fclose(fp);
        free(current_file);
    }

    XXH64_hash_t final = XXH3_64bits_digest(state);
    memcpy(out_hash, &final, BARR_XXHASH_LEN);

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
