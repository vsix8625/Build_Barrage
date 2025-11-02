#include "barr_cmd_play.h"
#include "barr_gc.h"
#include "barr_io.h"

barr_i32 BARR_command_play(barr_i32 argc, char **argv)
{
    if (argc < 2)
    {
        BARR_errlog("Usage: barr play <dirpath> [--shuffle]");
        return 1;
    }

    const char *dirpath = argv[1];
    bool shuffle = false;
    bool video = false;

    for (barr_i32 i = 2; i < argc; ++i)
    {
        if (BARR_strmatch(argv[i], "--shuffle"))
        {
            shuffle = true;
        }
        if (BARR_strmatch(argv[i], "--video"))
        {
            video = true;
        }
    }

    if (!BARR_is_installed("mpv"))
    {
        BARR_errlog("mpv not found! Please install mpv to play music.");
        return 1;
    }

    if (!BARR_isdir(dirpath))
    {
        BARR_errlog("Provided path is not a directory: %s", dirpath);
        return 1;
    }

    BARR_log("Launching mpv for directory: %s %s", dirpath, shuffle ? "(shuffle mode)" : "");

    barr_i32 max_args = 6;  // mpv + dir + optional shuffle + null
    char **exec_args = BARR_gc_alloc(sizeof(char *) * max_args);

    barr_i32 idx = 0;
    exec_args[idx++] = "mpv";

    if (shuffle)
    {
        exec_args[idx++] = "--shuffle";
    }

    exec_args[idx++] = (char *) dirpath;
    if (!video)
    {
        exec_args[idx++] = "--no-video";
    }
    exec_args[idx++] = "--really-quiet";
    exec_args[idx++] = NULL;

    BARR_run_process_BG(exec_args[0], exec_args);

    return 0;
}
