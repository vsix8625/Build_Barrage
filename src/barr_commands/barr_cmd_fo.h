#ifndef BARR_CMD_FO_H_
#define BARR_CMD_FO_H_

#include "barr_defs.h"

#define BARR_FO_DIR ".barr/fo"
#define BARR_FO_LOCK ".barr/fo/lock"

#define BARR_FO_REPORT_FILE ".barr/fo/report"
#define BARR_LOCAL_SHARE_DATA_DIR ".local/share/barr"
#define BARR_LOCAL_SHARE_BARR_HQ BARR_LOCAL_SHARE_DATA_DIR "/barr_hq"

barr_i32 BARR_command_fo(barr_i32 argc, char **argv);
char *BARR_get_value(const char *fpath, const char *key);

#endif  // BARR_CMD_FO_H_
