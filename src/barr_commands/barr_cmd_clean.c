#include "barr_cmd_clean.h"
#include "barr_io.h"
#include <time.h>

barr_i32 BARR_command_clean(barr_i32 argc, char **argv)
{
    BARR_VOID(argc);
    BARR_VOID(argv);
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    if (!BARR_is_initialized())
    {
        return 1;
    }

    if (BARR_isdir("build"))
    {
        BARR_rmrf("build");
        if (BARR_isfile(BARR_CACHE_FILE))
        {
            BARR_rmrf(BARR_CACHE_FILE);
        }
    }

    //----------------------------------------------------------------------------------------------------
    // CLOCK
    clock_gettime(CLOCK_MONOTONIC, &end);
    barr_i64 sec = (barr_i64) (end.tv_sec - start.tv_sec);
    barr_i64 nsec = (barr_i64) (end.tv_nsec - start.tv_nsec);
    if (nsec < 0)
    {
        --sec;
        nsec += 1000000000LL;
    }
    double elapsed = (double) sec + (double) nsec / 1e9;
    BARR_log("Clean completed! | Time: \033[34;1m %.6fs", elapsed);
    return 0;
}
