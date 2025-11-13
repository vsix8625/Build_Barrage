#ifndef BARR_MATH_H_
#define BARR_MATH_H_

#include "barr_defs.h"

#define BARR_MATH_IS_POW2(x) (((x) & ((x) - 1)) == 0)
#define BARR_MATH_DOUBLE(x) ((x) << 1)
#define BARR_MATH_HALF(x) ((x) >> 1)
#define BARR_MATH_MASK(cap) ((cap) - 1)

barr_u64 BARR_next_pow2(barr_u64 v);

#endif  // BARR_MATH_H_
