#ifndef BARR_SIMD_H_
#define BARR_SIMD_H_

#include "barr_defs.h"

size_t BARR_count_null_terminated_sse4_2(const char **arr);
size_t BARR_count_null_terminated_avx(const char **arr);

#endif  // BARR_SIMD_H_
