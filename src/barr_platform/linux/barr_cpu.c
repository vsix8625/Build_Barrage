#include "barr_cpu.h"
#include "barr_env.h"
#include "barr_io.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

barr_simd_level_t g_barr_simd_level = SIMD_NONE;

void BARR_init_simd(void)
{
    BARR_InfoCPU cpu;
    BARR_get_cpu_info(&cpu);
    g_barr_simd_level = cpu.simd;

    const char *name[] = {"NONE", "SSE2", "SSE4.2", "AVX", "AVX2"};
    if (g_barr_verbose)
    {
        BARR_log("[SIMD]: Detected: %s", name[g_barr_simd_level]);
    }
}

static barr_simd_level_t barr_detect_simd_from_flags(const char *flags_line)
{
    if (flags_line == NULL)
    {
        return SIMD_NONE;
    }

    // add avx512 in future
    if (strstr(flags_line, "avx2"))
    {
        return SIMD_AVX2;
    }
    if (strstr(flags_line, "avx"))
    {
        return SIMD_AVX;
    }
    if (strstr(flags_line, "sse4_2"))
    {
        return SIMD_SSE42;
    }
    if (strstr(flags_line, "sse2"))
    {
        return SIMD_SSE2;
    }
    return SIMD_NONE;
}

static size_t barr_parse_cache_size(const char *buf)
{
    size_t size = 0;
    char   unit = 0;

    if (sscanf(buf, "%zu%c", &size, &unit) == 2)
    {
        if (unit == 'K' || unit == 'k')
        {
            size *= 1024;
        }
        else if (unit == 'M' || unit == 'm')
        {
            size *= 1024UL * 1024UL;
        }
        else if (unit == 'G' || unit == 'g')
        {
            size *= 1024UL * 1024UL * 1024UL;
        }
    }

    return size;
}

static size_t barr_get_cache_size_sys(const char *wanted_type)
{
    char   path[BARR_BUF_SIZE_128];
    char   buf[BARR_BUF_SIZE_64];
    size_t size = 0;

    for (int i = 0; i < 8; i++)
    {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%d/type", i);
        FILE *type_fp = fopen(path, "r");
        if (!type_fp)
        {
            continue;
        }

        char type[BARR_BUF_SIZE_32] = {0};
        if (!fgets(type, sizeof(type), type_fp))
        {
            fclose(type_fp);
            continue;
        }
        fclose(type_fp);

        if (!strstr(type, wanted_type))
        {
            continue;
        }

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%d/size", i);
        FILE *fp = fopen(path, "r");
        if (!fp)
        {
            continue;
        }

        if (fgets(buf, sizeof(buf), fp))
        {
            size = barr_parse_cache_size(buf);
        }

        fclose(fp);
        if (size > 0)
        {
            break;
        }
    }

    return size;
}

void BARR_get_cpu_info(BARR_InfoCPU *info)
{
    if (info == NULL)
    {
        return;
    }

    memset(info, 0, sizeof(*info));

    info->threads = (barr_u32) sysconf(_SC_NPROCESSORS_ONLN);
    info->cores   = (barr_u32) sysconf(_SC_NPROCESSORS_CONF);
    info->simd    = SIMD_NONE;

    FILE *fp = fopen("/proc/cpuinfo", "r");

    if (fp == NULL)
    {
        // fallback
        info->cache_size = 2 * 1024 * 1024;
        return;
    }

    char     line[BARR_BUF_SIZE_1024];
    char     all_flags[BARR_BUF_SIZE_1024] = {0};
    barr_i32 flags_found                   = 0;

    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "model name", 10) == 0)
        {
            char *p = strchr(line, ':');
            if (p)
            {
                strncpy(info->model, p + 2, sizeof(info->model) - 1);
                size_t len = strlen(info->model);
                if (len > 0 && info->model[len - 1] == '\n')
                {
                    info->model[len - 1] = '\0';
                }
            }
        }
        else if (strncmp(line, "cpu MHz", 7) == 0)
        {
            double mhz = 0.0;
            if (sscanf(line, "cpu MHz\t: %lf", &mhz) == 1)
            {
                info->mhz = mhz;
            }
        }
        else if (strncmp(line, "cache size", 10) == 0 && info->cache_size == 0)
        {
            size_t size_kb = 0;
            if (sscanf(line, "cache size\t: %zu KB", &size_kb) == 1)
            {
                info->cache_size = size_kb * 1024UL;
            }
        }
        else if (strncmp(line, "flags", 5) == 0)
        {
            char *p = strchr(line, ':');
            if (p && !flags_found)
            {
                strncpy(all_flags, p + 2, sizeof(all_flags) - 1);
                all_flags[strcspn(all_flags, "\n")] = '\0';
                flags_found                         = 1;
            }
        }
    }
    fclose(fp);

    info->simd = barr_detect_simd_from_flags(all_flags);

    if (info->cache_size == 0)
    {
        info->cache_size = barr_get_cache_size_sys("Unified");
    }

    if (info->cache_size == 0)
    {
        info->cache_size = barr_get_cache_size_sys("Data");
    }

    if (info->cache_size == 0)
    {
        size_t threads   = (size_t) (info->threads > 0 ? info->threads : 4);
        info->cache_size = threads * 2UL * 1024UL * 1024UL;
    }

    if (info->cache_size < (1UL << 20))  // <1MB
    {
        info->cache_size = 1UL << 20;
    }
    else if (info->cache_size > (256UL << 20))  // >256MB
    {
        info->cache_size = 256UL << 20;
    }
}

// Governors

#define GOVERNOR_DIR     "/sys/devices/system/cpu"
#define GOVERNOR_PERF    "performance"
#define GOVERNOR_SCHUTIL "schedutil"

static barr_i32 barr_gov_write(const char *cpu_name, const char *gov)
{
    char path[BARR_BUF_SIZE_512];
    snprintf(path, sizeof(path), "%s/%s/cpufreq/scaling_governor", GOVERNOR_DIR, cpu_name);

    barr_i32 fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        return 0;
    }

    ssize_t n = write(fd, gov, strlen(gov));

    close(fd);
    return n == (ssize_t) strlen(gov);
}

static barr_i32 barr_gov_read(const char *cpu_name, char *buf, size_t buf_size)
{
    char path[BARR_BUF_SIZE_512];
    snprintf(path, sizeof(path), "%s/%s/cpufreq/scaling_governor", GOVERNOR_DIR, cpu_name);

    barr_i32 fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        return 0;
    }

    ssize_t n = read(fd, buf, buf_size - 1);
    close(fd);

    if (n <= 0)
    {
        return 0;
    }

    buf[n] = '\0';

    char *nl = strchr(buf, '\n');
    if (nl)
    {
        *nl = '\0';
    }

    return 1;
}

bool BARR_cpu_perf(void)
{
    DIR *dir = opendir(GOVERNOR_DIR);
    if (dir == NULL)
    {
        return false;
    }

    struct dirent *ent;
    barr_i32       changed = 0;

    while ((ent = readdir(dir)))
    {
        if (strncmp(ent->d_name, "cpu", 3) != 0)
        {
            continue;
        }

        char current[BARR_BUF_SIZE_32];
        if (!barr_gov_read(ent->d_name, current, sizeof(current)))
        {
            continue;
        }

        if (BARR_strmatch(current, GOVERNOR_PERF))
        {
            continue;
        }

        if (barr_gov_write(ent->d_name, GOVERNOR_PERF))
        {
            changed++;
        }
    }

    closedir(dir);
    return changed > 0;
}

bool BARR_cpu_norm(void)
{
    DIR *dir = opendir(GOVERNOR_DIR);
    if (dir == NULL)
    {
        return false;
    }
    struct dirent *ent;
    barr_i32       changed = 0;

    while ((ent = readdir(dir)))
    {
        if (strncmp(ent->d_name, "cpu", 3) != 0)
        {
            continue;
        }

        if (barr_gov_write(ent->d_name, GOVERNOR_SCHUTIL))
        {
            changed++;
        }
    }

    closedir(dir);
    return changed > 0;
}
