#ifndef BARR_ENV_H_
#define BARR_ENV_H_

#include "barr_cpu.h"

const char *BARR_get_config(const char *type);

extern bool g_barr_silent_logs;
extern bool g_barr_verbose;
extern barr_simd_level_t g_barr_simd_level;

#endif  // BARR_ENV_H_
