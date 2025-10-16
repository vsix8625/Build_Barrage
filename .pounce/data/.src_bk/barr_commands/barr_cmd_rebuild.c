#include "barr_cmd_rebuild.h"
#include "barr_cmd_build.h"
#include "barr_cmd_clean.h"
#include "barr_io.h"
#include <time.h>

barr_i32 BARR_command_rebuild(barr_i32 argc, char **argv)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    if (!BARR_is_initialized())
    {
        return 1;
    }

    if (BARR_command_clean(argc, argv))
    {
        BARR_warnlog("BARR_command_clean() abnormal exit");
    }
    if (BARR_command_build(argc, argv))
    {
        BARR_warnlog("BARR_command_build() abnormal exit");
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
    BARR_log("Re-build completed! | Time: \033[34;1m %.6fs", elapsed);
    return 0;
}
