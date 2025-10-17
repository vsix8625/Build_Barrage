#ifndef BARR_CMD_MODE_H_
#define BARR_CMD_MODE_H_

#include "barr_defs.h"

#define BARR_LOCAL_SHARE_MODES_ACTIVE_FILE ".local/share/barr/modes/active"

bool BARR_is_mode_active(const char *expected);
barr_i32 BARR_command_mode(barr_i32 argc, char **argv);

#endif  // BARR_CMD_MODE_H_
