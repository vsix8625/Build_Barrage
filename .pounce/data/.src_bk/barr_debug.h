#ifndef BARR_DEBUG_H_
#define BARR_DEBUG_H_

#include "barr_defs.h"

#define BARR_DEBUG_LOG_FILE

void BARR_dbglog(const char *format, ...) BARR_LOG_VA_CHECK(1);
void BARR_dumb_backtrace(void);

#define BARR_PROFILE_RET(fn, result)                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        struct timespec start, end;                                                                                    \
        clock_gettime(CLOCK_MONOTONIC, &start);                                                                        \
                                                                                                                       \
        result = fn;                                                                                                   \
                                                                                                                       \
        clock_gettime(CLOCK_MONOTONIC, &end);                                                                          \
                                                                                                                       \
        barr_u64 ns = (barr_u64) (end.tv_sec - start.tv_sec) * 1000000000ULL + (barr_u64) (end.tv_nsec - start.tv_nsec);  \
        if (ns >= 1000000ULL)                                                                                          \
            BARR_log("[Profiler]%s → \033[34;1m(%.2f ms)", #fn, ns / 1000000.0);                                        \
        else if (ns >= 1000ULL)                                                                                        \
            BARR_log("[Profiler]%s → \033[34;1m(%.2f microsec)", #fn, ns / 1000000.0);                                  \
        else                                                                                                           \
            BARR_log("[Profiler]%s → \033[34;1m(%.2f ns)", #fn, ns / 1000000.0);                                        \
    } while (0)

#define BARR_PROFILE_CALL(fn)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        struct timespec start, end;                                                                                    \
        clock_gettime(CLOCK_MONOTONIC, &start);                                                                        \
                                                                                                                       \
        fn;                                                                                                            \
                                                                                                                       \
        clock_gettime(CLOCK_MONOTONIC, &end);                                                                          \
                                                                                                                       \
        barr_u64 ns = (barr_u64) (end.tv_sec - start.tv_sec) * 1000000000ULL + (barr_u64) (end.tv_nsec - start.tv_nsec);  \
        if (ns >= 1000000ULL)                                                                                          \
            BARR_log("[Profiler]%s → \033[34;1m(%.2f ms)", #fn, ns / 1000000.0);                                        \
        else if (ns >= 1000ULL)                                                                                        \
            BARR_log("[Profiler]%s → \033[34;1m(%.2f microsec)", #fn, ns / 1000000.0);                                  \
        else                                                                                                           \
            BARR_log("[Profiler]%s → \033[34;1m(%.2f ns)", #fn, ns / 1000000.0);                                        \
    } while (0)

#endif  // BARR_DEBUG_H_
