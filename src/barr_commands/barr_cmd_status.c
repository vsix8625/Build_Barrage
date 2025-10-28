#include "barr_cmd_status.h"
#include "barr_io.h"

barr_i32 BARR_cmd_status(barr_i32 argc, char **argv)
{
    BARR_VOID(argc);
    BARR_VOID(argv);
    barr_i32 result = 0;

    BARR_printf("--------------------------------------------------------------------------------------\n");

    BARR_InitStatus res = BARR_check_initialized();

    switch (res)
    {
        case BARR_INIT_NOT_FOUND:
        {
            // BARR_is_initialized() already prints logs if you call it
            BARR_is_initialized();  // will log errors/warnings
            result = 1;
            break;
        }

        case BARR_INIT_LOCK_NEWER:
        {
            BARR_is_initialized();  // logs error about newer lock
            result = 1;
            break;
        }

        case BARR_INIT_LOCK_OLDER:
        {
            BARR_is_initialized();  // logs warning
            break;
        }

        case BARR_INIT_OK:
        {
            BARR_log("Build Barrage version %d.%d.%d initialized in %s", BARR_VERSION_MAJOR, BARR_VERSION_MINOR,
                     BARR_VERSION_PATCH, BARR_getcwd());
            break;
        }

        default:
        {
            BARR_errlog("Unknown initialization state!");
            result = 1;
            break;
        }
    }

    BARR_printf("--------------------------------------------------------------------------------------\n");
    return result;
}
