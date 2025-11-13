#include "barr_math.h"

barr_u64 BARR_next_pow2(barr_u64 v)
{
    if (v == 0)
    {
        return 1;
    }
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
}
