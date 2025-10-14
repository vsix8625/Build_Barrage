#include "crx.h"

ATL_Arena g_crx_arena;

bool CRX_init(void)
{
    ATL_arena_init(&g_crx_arena, CRX_GLOB_ARENA_SIZE, "g_crx_arena", 8);
    return true;
}

bool CRX_close(void)
{
    ATL_destroy_arena(&g_crx_arena);
    return true;
}
