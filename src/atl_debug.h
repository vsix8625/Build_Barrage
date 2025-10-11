#ifndef ATL_DEBUG_H_
#define ATL_DEBUG_H_

#include "atl_defs.h"

#define ATL_DEBUG_LOG_FILE

void ATL_dbglog(const char *format, ...) ATL_LOG_VA_CHECK(1);
void ATL_dumb_backtrace(void);

#define ATL_PROFILE_RET(fn, result)                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        struct timespec start, end;                                                                                    \
        clock_gettime(CLOCK_MONOTONIC, &start);                                                                        \
                                                                                                                       \
        result = fn;                                                                                                   \
                                                                                                                       \
        clock_gettime(CLOCK_MONOTONIC, &end);                                                                          \
                                                                                                                       \
        atl_u64 ns = (atl_u64) (end.tv_sec - start.tv_sec) * 1000000000ULL + (atl_u64) (end.tv_nsec - start.tv_nsec);  \
        if (ns >= 1000000ULL)                                                                                          \
            ATL_log("[Profiler]%s → \033[34;1m(%.2f ms)", #fn, ns / 1000000.0);                                        \
        else if (ns >= 1000ULL)                                                                                        \
            ATL_log("[Profiler]%s → \033[34;1m(%.2f microsec)", #fn, ns / 1000000.0);                                  \
        else                                                                                                           \
            ATL_log("[Profiler]%s → \033[34;1m(%.2f ns)", #fn, ns / 1000000.0);                                        \
    } while (0)

#define ATL_PROFILE_CALL(fn)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        struct timespec start, end;                                                                                    \
        clock_gettime(CLOCK_MONOTONIC, &start);                                                                        \
                                                                                                                       \
        fn;                                                                                                            \
                                                                                                                       \
        clock_gettime(CLOCK_MONOTONIC, &end);                                                                          \
                                                                                                                       \
        atl_u64 ns = (atl_u64) (end.tv_sec - start.tv_sec) * 1000000000ULL + (atl_u64) (end.tv_nsec - start.tv_nsec);  \
        if (ns >= 1000000ULL)                                                                                          \
            ATL_log("[Profiler]%s → \033[34;1m(%.2f ms)", #fn, ns / 1000000.0);                                        \
        else if (ns >= 1000ULL)                                                                                        \
            ATL_log("[Profiler]%s → \033[34;1m(%.2f microsec)", #fn, ns / 1000000.0);                                  \
        else                                                                                                           \
            ATL_log("[Profiler]%s → \033[34;1m(%.2f ns)", #fn, ns / 1000000.0);                                        \
    } while (0)

#endif  // ATL_DEBUG_H_
