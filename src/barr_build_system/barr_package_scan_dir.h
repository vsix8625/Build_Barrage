#ifndef BARR_PACKAGE_SCAN_DIR_H_
#define BARR_PACKAGE_SCAN_DIR_H_

#include "barr_find_package.h"
#include "barr_os_layer.h"

void BARR_package_scan_dir(BARR_PackageInfo *out, const char *dirpath, const char *target_pkg);

#endif  // BARR_PACKAGE_SCAN_DIR_H_
