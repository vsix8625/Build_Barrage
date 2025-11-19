#include "barr_cmd_fo.h"
#include "barr_cmd_version.h"
#include "barr_io.h"
#include <time.h>

static void barr_fo_on(void)
{
    time_t tstamp = time(NULL);

    if (!BARR_isdir(BARR_FO_DIR))
    {
        BARR_mkdir_p(BARR_FO_DIR);
    }

    BARR_file_write(BARR_FO_REPORT_FILE, "status = ON\ntimestamp = %ld\n", tstamp);
}

static void barr_fo_off(void)
{
    time_t tstamp = time(NULL);

    if (!BARR_isdir(BARR_FO_DIR) || !BARR_isfile(BARR_FO_REPORT_FILE))
    {
        return;
    }

    BARR_file_write(BARR_FO_REPORT_FILE, "status = OFF\ntimestamp = %ld\n", tstamp);
}

void BARR_fo_watch_job(void)
{
}

barr_i32 BARR_command_fo(barr_i32 argc, char **argv)
{
    if (!BARR_init())
    {
        return 1;
    }

    for (size_t i = 1; i < argc; ++i)
    {
        const char *opt = argv[i];

        if (BARR_strmatch(opt, "ON"))
        {
            BARR_log("Forward observer deployed");
            barr_fo_on();
            // TODO: get the barr installed DIR
            char *args[] = {"/devenv/repos/Build_Barrage/tools/fo/build/fo/debug/bin/fo", NULL};
            BARR_run_process_BG(args[0], args);
            return 0;
        }

        if (BARR_strmatch(opt, "OFF"))
        {
            barr_fo_off();
            return 0;
        }

        if (BARR_strmatch(opt, "REPORT"))
        {
        }
    }

    return 0;
}
