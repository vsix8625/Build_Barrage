#include "atl_sha256.h"
#include "atl_io.h"

#include <ctype.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>

bool ATL_hash_file_sha256(const char *filepath, atl_u8 out_hash[ATL_SHA256_LEN])
{
    FILE *fp = fopen(filepath, "rb");
    if (!fp)
    {
        ATL_errlog("%s(): failed to open \"%s\" to read", __func__, filepath);
        return false;
    }

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx)
    {
        ATL_errlog("%s(): failed to allocate EVP_MD_CTX", __func__);
        fclose(fp);
        return false;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1)
    {
        ATL_errlog("%s(): failed to initialize SHA-256 digest", __func__);
        EVP_MD_CTX_free(mdctx);
        fclose(fp);
        return false;
    }

    atl_u8 buf[16 * ATL_BUF_SIZE_1024];
    size_t n;
    bool success = true;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
    {
        if (EVP_DigestUpdate(mdctx, buf, n) != 1)
        {
            ATL_errlog("%s(): failed to update digest with file data", __func__);
            success = false;
            break;
        }
    }

    if (!success || ferror(fp))
    {
        if (ferror(fp))
        {
            ATL_errlog("%s(): error reading file \"%s\"", __func__, filepath);
        }
        EVP_MD_CTX_free(mdctx);
        fclose(fp);
        return false;
    }

    fclose(fp);

    // finalize and clean up

    atl_u32 len = 0;
    if (EVP_DigestFinal_ex(mdctx, out_hash, &len) != 1)
    {
        ATL_errlog("%s(): failed to finalize digest", __func__);
        success = false;
    }

    EVP_MD_CTX_free(mdctx);

    if (success && len != ATL_SHA256_LEN)
    {
        ATL_errlog("%s(): unexpected sha length. Expected: %d got %u", __func__, ATL_SHA256_LEN, len);
        success = false;
    }

    return success;
}

bool ATL_hash_includes_sha256(const char *filepath, atl_u8 out_hash[ATL_SHA256_LEN])
{
    FILE *fp = fopen(filepath, "r");
    if (!fp)
    {
        ATL_errlog("%s(): failed to open \"%s\" to read", __func__, filepath);
        return false;
    }

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx)
    {
        ATL_errlog("%s(): failed to allocate EVP_MD_CTX", __func__);
        fclose(fp);
        return false;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1)
    {
        ATL_errlog("%s(): failed to initialize SHA-256 digest", __func__);
        EVP_MD_CTX_free(mdctx);
        fclose(fp);
        return false;
    }

    char line[ATL_BUF_SIZE_1024];
    while (fgets(line, sizeof(line), fp))
    {
        char *p = line;
        while (isspace((atl_u8) *p))
        {
            p++;
        }

        if (strncmp(p, "#include", 8) == 0)
        {
            p += 8;
            while (isspace((atl_u8) *p))
            {
                p++;
            }

            if (*p == '\"')
            {
                p++;
                char include_file[ATL_BUF_SIZE_512];
                atl_i32 i = 0;
                while (*p && *p != '\"' && i < (atl_i32) sizeof(include_file) - 1)
                {
                    include_file[i++] = *p++;
                }
                include_file[i] = '\0';

                atl_u8 include_hash[ATL_SHA256_LEN];
                if (ATL_hash_file_sha256(include_file, include_hash))
                {
                    if (EVP_DigestUpdate(mdctx, include_hash, ATL_SHA256_LEN))
                    {
                        fclose(fp);
                        EVP_MD_CTX_free(mdctx);
                        return false;
                    }
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

bool ATL_hash_flags_sha256(const char *flags, atl_u8 out_hash[ATL_SHA256_LEN])
{
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx)
    {
        ATL_errlog("%s(): failed to allocate EVP_MD_CTX", __func__);
        return false;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1)
    {
        ATL_errlog("%s(): failed to initialize SHA-256 digest", __func__);
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    if (EVP_DigestUpdate(mdctx, flags, strlen(flags)) != 1)
    {
        ATL_errlog("%s(): failed to update digest", __func__);
        return false;
    }

    if (EVP_DigestFinal_ex(mdctx, out_hash, NULL) != 1)
    {
        ATL_errlog("%s(): failed to finalize digest", __func__);
        return false;
    }

    EVP_MD_CTX_free(mdctx);
    return true;
}

bool ATL_hashes_merge_sha256(atl_u8 file_hash[ATL_SHA256_LEN], atl_u8 include_hash[ATL_SHA256_LEN],
                             atl_u8 flags_hash[ATL_SHA256_LEN], atl_u8 out_hash[ATL_SHA256_LEN])
{
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx)
    {
        ATL_errlog("%s(): failed to allocate EVP_MD_CTX", __func__);
        return false;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1)
    {
        ATL_errlog("%s(): failed to initialize SHA-256 digest", __func__);
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    // update digest with all hashes
    if (EVP_DigestUpdate(mdctx, file_hash, ATL_SHA256_LEN) != 1)
    {
        ATL_errlog("%s(): failed to update digest with file_hash", __func__);
        return false;
    }
    if (EVP_DigestUpdate(mdctx, include_hash, ATL_SHA256_LEN) != 1)
    {
        ATL_errlog("%s(): failed to update digest with include_hash", __func__);
        return false;
    }
    if (EVP_DigestUpdate(mdctx, flags_hash, ATL_SHA256_LEN) != 1)
    {
        ATL_errlog("%s(): failed to update digest with flags_hash", __func__);
        return false;
    }

    // out hash final
    if (EVP_DigestFinal_ex(mdctx, out_hash, NULL) != 1)
    {
        ATL_errlog("%s(): failed to finalize digest", __func__);
        return false;
    }

    EVP_MD_CTX_free(mdctx);
    return true;
}
