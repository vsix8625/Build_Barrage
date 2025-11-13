#ifndef BARR_CPU_H_
#define BARR_CPU_H_

#include "barr_defs.h"

typedef enum
{
    SIMD_NONE,
    SIMD_SSE2,
    SIMD_SSE42,
    SIMD_AVX,
    SIMD_AVX2,
} barr_simd_level_t;

typedef struct BARR_InfoCPU
{
    barr_u32 cores;
    barr_u32 threads;
    double mhz;
    size_t cache_size;
    char model[BARR_BUF_SIZE_128];
    barr_simd_level_t simd;
} BARR_InfoCPU;

void BARR_get_cpu_info(BARR_InfoCPU *info);
void BARR_init_simd(void);

#endif  // BARR_CPU_H_
