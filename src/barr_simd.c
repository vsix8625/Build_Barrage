#include "barr_simd.h"
#include <immintrin.h>

size_t BARR_count_null_terminated_sse4_2(const char **arr)
{
    if (arr == NULL)
    {
        return 0;
    }

    const __m128i zero = _mm_setzero_si128();
    size_t count = 0;

    while (((uintptr_t) arr & 15) != 0)
    {
        if (*arr == NULL)
        {
            return count;
        }
        count++;
        arr++;
    }

    for (;;)
    {
        __m128i vec = _mm_load_si128((const __m128i *) arr);
        __m128i cmp = _mm_cmpeq_epi64(vec, zero);

        barr_i32 mask = _mm_movemask_pd(_mm_castsi128_pd(cmp));
        if (mask != 0)
        {
            barr_i32 idx = __builtin_ctz(mask);
            return count + idx;
        }

        count += 2;
        arr += 2;
    }

    return count;
}

size_t BARR_count_null_terminated_avx(const char **arr)
{
    if (arr == NULL)
    {
        return 0;
    }

    size_t count = 0;

    while (((uintptr_t) arr & 31) != 0)
    {
        if (*arr == NULL)
        {
            return count;
        }
        count++;
        arr++;
    }

    const __m256d zero = _mm256_setzero_pd();

    for (;;)
    {
        __m256d vec = _mm256_load_pd((const double *) arr);

        __m256d cmp = _mm256_cmp_pd(vec, zero, _CMP_EQ_OQ);

        int mask = _mm256_movemask_pd(cmp);

        if (mask != 0)
        {
            int idx = __builtin_ctz(mask);
            return count + idx;
        }

        count += 4;
        arr += 4;
    }

    return count;
}
