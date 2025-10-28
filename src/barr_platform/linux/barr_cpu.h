#ifndef BARR_CPU_H_
#define BARR_CPU_H_

#include "barr_defs.h"

typedef struct BARR_InfoCPU
{
    barr_i32 cores;
    barr_i32 threads;
    double mhz;
    size_t cache_size;
    char model[BARR_BUF_SIZE_128];
} BARR_InfoCPU;

void BARR_get_cpu_info(BARR_InfoCPU *info);

#endif  // BARR_CPU_H_
