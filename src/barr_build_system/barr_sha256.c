#include "barr_sha256.h"
#include "barr_io.h"

#include <ctype.h>
#include <libgen.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>

bool BARR_hash_file_sha256(const char *filepath, barr_u8 out_hash[BARR_SHA256_LEN])
{
    FILE *fp = fopen(filepath, "rb");
    if (!fp)
    {
        BARR_errlog("%s(): failed to open \"%s\" to read", __func__, filepath);
        return false;
    }

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx)
    {
        BARR_errlog("%s(): failed to allocate EVP_MD_CTX", __func__);
        fclose(fp);
        return false;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1)
    {
        BARR_errlog("%s(): failed to initialize SHA-256 digest", __func__);
        EVP_MD_CTX_free(mdctx);
        fclose(fp);
        return false;
    }

    barr_u8 buf[16 * BARR_BUF_SIZE_1024];
    size_t n;
    bool success = true;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
    {
        if (EVP_DigestUpdate(mdctx, buf, n) != 1)
        {
            BARR_errlog("%s(): failed to update digest with file data", __func__);
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
        EVP_MD_CTX_free(mdctx);
        fclose(fp);
        return false;
    }

    fclose(fp);

    // finalize and clean up

    barr_u32 len = 0;
    if (EVP_DigestFinal_ex(mdctx, out_hash, &len) != 1)
    {
        BARR_errlog("%s(): failed to finalize digest", __func__);
        success = false;
    }

    EVP_MD_CTX_free(mdctx);

    if (success && len != BARR_SHA256_LEN)
    {
        BARR_errlog("%s(): unexpected sha length. Expected: %d got %u", __func__, BARR_SHA256_LEN, len);
        success = false;
    }

    return success;
}

bool BARR_hash_includes_sha256(const char *filepath, barr_u8 out_hash[BARR_SHA256_LEN])
{
    FILE *fp = fopen(filepath, "r");
    if (!fp)
    {
        BARR_errlog("%s(): failed to open \"%s\" to read", __func__, filepath);
        return false;
    }

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx)
    {
        BARR_errlog("%s(): failed to allocate EVP_MD_CTX", __func__);
        fclose(fp);
        return false;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1)
    {
        BARR_errlog("%s(): failed to initialize SHA-256 digest", __func__);
        EVP_MD_CTX_free(mdctx);
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

                barr_u8 include_hash[BARR_SHA256_LEN];
                if (BARR_hash_file_sha256(full_include_path, include_hash))
                {
                    if (EVP_DigestUpdate(mdctx, include_hash, BARR_SHA256_LEN) != 1)
                    {
                        fclose(fp);
                        EVP_MD_CTX_free(mdctx);
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

    if (EVP_DigestFinal_ex(mdctx, out_hash, NULL) != 1)
    {
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    EVP_MD_CTX_free(mdctx);
    return true;
}

bool BARR_hash_flags_sha256(const char *flags, barr_u8 out_hash[BARR_SHA256_LEN])
{
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx)
    {
        BARR_errlog("%s(): failed to allocate EVP_MD_CTX", __func__);
        return false;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1)
    {
        BARR_errlog("%s(): failed to initialize SHA-256 digest", __func__);
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    if (EVP_DigestUpdate(mdctx, flags, strlen(flags)) != 1)
    {
        BARR_errlog("%s(): failed to update digest", __func__);
        return false;
    }

    if (EVP_DigestFinal_ex(mdctx, out_hash, NULL) != 1)
    {
        BARR_errlog("%s(): failed to finalize digest", __func__);
        return false;
    }

    EVP_MD_CTX_free(mdctx);
    return true;
}

bool BARR_hashes_merge_sha256(barr_u8 file_hash[BARR_SHA256_LEN], barr_u8 include_hash[BARR_SHA256_LEN],
                             barr_u8 flags_hash[BARR_SHA256_LEN], barr_u8 out_hash[BARR_SHA256_LEN])
{
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx)
    {
        BARR_errlog("%s(): failed to allocate EVP_MD_CTX", __func__);
        return false;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1)
    {
        BARR_errlog("%s(): failed to initialize SHA-256 digest", __func__);
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    // update digest with all hashes
    if (EVP_DigestUpdate(mdctx, file_hash, BARR_SHA256_LEN) != 1)
    {
        BARR_errlog("%s(): failed to update digest with file_hash", __func__);
        return false;
    }
    if (EVP_DigestUpdate(mdctx, include_hash, BARR_SHA256_LEN) != 1)
    {
        BARR_errlog("%s(): failed to update digest with include_hash", __func__);
        return false;
    }
    if (EVP_DigestUpdate(mdctx, flags_hash, BARR_SHA256_LEN) != 1)
    {
        BARR_errlog("%s(): failed to update digest with flags_hash", __func__);
        return false;
    }

    // out hash final
    if (EVP_DigestFinal_ex(mdctx, out_hash, NULL) != 1)
    {
        BARR_errlog("%s(): failed to finalize digest", __func__);
        return false;
    }

    EVP_MD_CTX_free(mdctx);
    return true;
}
