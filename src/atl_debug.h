#ifndef ATL_DEBUG_H_
#define ATL_DEBUG_H_

#include "atl_defs.h"

#define ATL_DEBUG_LOG_FILE

void ATL_dbglog(const char *format, ...) ATL_LOG_VA_CHECK(1);

#endif  // ATL_DEBUG_H_
