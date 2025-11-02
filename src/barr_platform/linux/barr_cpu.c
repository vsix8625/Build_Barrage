#include "barr_cpu.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static size_t barr_parse_cache_size(const char *buf)
{
    size_t size = 0;
    char unit = 0;

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
    char path[BARR_BUF_SIZE_128];
    char buf[BARR_BUF_SIZE_64];
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

    info->threads = (barr_i32) sysconf(_SC_NPROCESSORS_ONLN);
    info->cores = (barr_i32) sysconf(_SC_NPROCESSORS_CONF);

    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (fp)
    {
        char line[BARR_BUF_SIZE_1024];
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
        }
        fclose(fp);
    }

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
        size_t threads = (size_t) (info->threads > 0 ? info->threads : 4);
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
