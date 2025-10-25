#ifndef BARR_FIND_PACKAGE_H_
#define BARR_FIND_PACKAGE_H_

#include "barr_os_layer.h"

typedef struct BARR_PackageInfo
{
    const char *name;
    const char *version;
    const char *libs;
    const char *libs_static;
    const char *cflags;
    const char *libdir;
    const char *sharedlibdir;
    const char *includedir;
    const char *pkg_path;
} BARR_PackageInfo;

typedef struct BARR_PkgCacheNode
{
    const char *name;
    BARR_PackageInfo *info;
    struct BARR_PkgCacheNode *next;
} BARR_PkgCacheNode;

BARR_PackageInfo *BARR_find_package(const char *pkg, BARR_PackageInfo *out, bool is_static, const char *search_path);

BARR_PackageInfo *BARR_pkg_cache_get(const char *name);
void BARR_pkg_cache_set(const char *name, BARR_PackageInfo *info);

#endif  // BARR_FIND_PACKAGE_H_
