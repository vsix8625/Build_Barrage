#ifndef CRX_H_
#define CRX_H_

#include "barr_arena.h"

extern BARR_Arena g_crx_arena;

#define CRX_GLOB_ARENA_SIZE (6 * 1024)

bool CRX_init(void);
bool CRX_close(void);

#endif  // CRX_H_
