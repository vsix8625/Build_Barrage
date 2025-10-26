#include "olmos.h"
#include "barr_io.h"

bool OLM_init(void)
{
    BARR_log("Olmos system initialized");
    return true;
}

bool OLM_close(void)
{
    BARR_log("Olmos system shutdown");
    return true;
}
