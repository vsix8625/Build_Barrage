#include "barr_find_package.h"
#include "barr_debug.h"
#include "barr_gc.h"
#include "barr_io.h"
#include "barr_package_scan_dir.h"

#include <stdlib.h>
#include <string.h>

//----------------------------------------------------------------------------------------------------

BARR_PackageInfo *BARR_find_package(const char *pkg, BARR_PackageInfo *out, bool is_static, const char *search_path)
{
    if (!pkg || !out)
    {
        return NULL;
    }

    memset(out, 0, sizeof(*out));

    if (search_path)
    {
        BARR_package_scan_dir(out, search_path, pkg);
    }
    else
    {
        const char *home = BARR_GET_HOME();
        const char *sys_paths[] = {"/usr/local/lib/pkgconfig",
                                   "/usr/lib/pkgconfig",
                                   "/usr/share/pkgconfig",
                                   "/usr/lib/x86_64-linux-gnu/pkgconfig",  // Common for Debian/Ubuntu
                                   "/usr/lib64/pkgconfig",                 // Common for Fedora/CentOS
                                   "/usr/local/lib64/pkgconfig",           // For some custom installs
                                   "/opt/lib/pkgconfig",                   // For custom or Homebrew-like setups
                                   "/usr/local",
                                   "/usr",
                                   home,
                                   NULL};

        for (const char **p = sys_paths; *p; ++p)
        {
            BARR_package_scan_dir(out, *p, pkg);
            if (out->name)  // found
            {
                break;
            }
        }
    }

    if (!out->name)
    {
        BARR_warnlog("Falling back to pkg-config for %s", pkg);

        char *argv[BARR_BUF_SIZE_32];
        barr_i32 idx = 0;
        argv[idx++] = "pkg-config";
        argv[idx++] = "--cflags";
        argv[idx++] = "--libs";
        if (is_static)
        {
            argv[idx++] = "--static";
        }
        argv[idx++] = (char *) pkg;
        argv[idx++] = NULL;

        char *output = BARR_run_process_capture(argv);
        if (output && output[0])
        {
            // Split into cflags and libs
            char *libs = strstr(output, "-l");
            if (libs)
            {
                *libs = '\0';
                while (libs > output && *(libs - 1) == ' ')
                {
                    *(libs - 1) = '\0';
                }
                out->cflags = BARR_gc_strdup(output);
                out->libs = BARR_gc_strdup(libs);
                out->name = BARR_gc_strdup(pkg);

                char *ver_argv[] = {"pkg-config", "--modversion", (char *) pkg, NULL};
                char *version = BARR_run_process_capture(ver_argv);
                out->version = BARR_gc_strdup(version && version[0] ? version : "unknown");
                out->pkg_path = BARR_gc_strdup("pkg-config-fallback");
            }
            else
            {
                BARR_errlog("Invalid pkg-config output for %s: %s\n", pkg, output);
            }
        }
        else
        {
            BARR_errlog("Failed to run pkg-config for %s", pkg);
        }
    }

    if (!out->name)
    {
        BARR_errlog("Package '%s' not found!", pkg);
        return NULL;
    }

    if (is_static && out->libs_static)
    {
        out->libs = out->libs_static;
    }

    BARR_log("Found package: %s version %s | path: %s", out->name, out->version, out->pkg_path);
    BARR_log("CFLAGS: %s", out->cflags);
    BARR_log("LIBS: %s", out->libs);

    return out;
}

//----------------------------------------------------------------------------------------------------
