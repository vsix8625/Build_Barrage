#include "atl_cmd_status.h"
#include "atl_io.h"
#include <stdlib.h>

atl_i32 ATL_cmd_status(atl_i32 argc, char **argv)
{
    ATL_VOID(argc);
    ATL_VOID(argv);
    atl_i32 result = 0;

    ATL_printf("----------------------------------------STATUS----------------------------------------\n");

    ATL_InitStatus res = ATL_check_initialized();

    switch (res)
    {
        case ATL_INIT_NOT_FOUND:
        {
            // ATL_is_initialized() already prints logs if you call it
            ATL_is_initialized();  // will log errors/warnings
            result = 1;
            break;
        }

        case ATL_INIT_LOCK_NEWER:
        {
            ATL_is_initialized();  // logs error about newer lock
            result = 1;
            break;
        }

        case ATL_INIT_LOCK_OLDER:
        {
            ATL_is_initialized();  // logs warning
            break;
        }

        case ATL_INIT_OK:
        {
            ATL_is_initialized();  // logs normal init
            break;
        }

        default:
        {
            ATL_errlog("Unknown initialization state!");
            result = 1;
            break;
        }
    }

    ATL_printf("--------------------------------------------------------------------------------------\n");
    return result;
}
