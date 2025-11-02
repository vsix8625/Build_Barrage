#include "barr_cmd_rebuild.h"
#include "barr_cmd_build.h"
#include "barr_cmd_clean.h"
#include "barr_io.h"

barr_i32 BARR_command_rebuild(barr_i32 argc, char **argv)
{
    char *clean_argv[2];
    clean_argv[0] = "clean";
    clean_argv[1] = "-nc";
    if (BARR_command_clean(2, clean_argv))
    {
        BARR_warnlog("BARR_command_clean() abnormal exit");
    }
    if (BARR_command_build(argc, argv))
    {
        BARR_warnlog("BARR_command_build() abnormal exit");
    }
    return 0;
}
