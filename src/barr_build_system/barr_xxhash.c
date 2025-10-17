#include "barr_xxhash.h"
#include "barr_io.h"

#include <ctype.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>

#include <xxhash.h>

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
    FILE *fp = fopen(filepath, "r");
    if (!fp)
    {
        BARR_errlog("%s(): failed to open \"%s\" to read", __func__, filepath);
        return false;
    }

    XXH3_state_t *state = XXH3_createState();
    if (!state)
    {
        BARR_errlog("%s(): failed to allocate XXH3 state", __func__);
        fclose(fp);
        return false;
    }

    if (XXH3_64bits_reset(state) == XXH_ERROR)
    {
        BARR_errlog("%s(): failed to initialize XXH3", __func__);
        XXH3_freeState(state);
        fclose(fp);
        return false;
    }

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
                while (*p && *p != '\"' && i < (barr_i32) sizeof(include_file) - 1)
                {
                    include_file[i++] = *p++;
                }
                include_file[i] = '\0';

                char full_include_path[BARR_PATH_MAX];
                char filepath_copy[BARR_PATH_MAX];
                strncpy(filepath_copy, filepath, sizeof(filepath_copy));
                filepath_copy[sizeof(filepath_copy) - 1] = '\0';

                char *dir = dirname(filepath_copy);
                snprintf(full_include_path, sizeof(full_include_path), "%s/%s", dir, include_file);

                barr_u8 include_hash[BARR_XXHASH_LEN];
                if (BARR_hash_file_xxh3(full_include_path, include_hash))
                {
                    if (XXH3_64bits_update(state, include_hash, BARR_XXHASH_LEN) == XXH_ERROR)
                    {
                        BARR_errlog("%s(): failed to update XXH3 with include hash", __func__);
                        fclose(fp);
                        XXH3_freeState(state);
                        return false;
                    }
                }
                else
                {
                    BARR_warnlog("%s(): failed to hash include \"%s\"", __func__, include_file);
                }
            }
        }
    }

    fclose(fp);

    XXH64_hash_t hash = XXH3_64bits_digest(state);
    XXH3_freeState(state);
    memcpy(out_hash, &hash, BARR_XXHASH_LEN);

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
