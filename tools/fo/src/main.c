#define _POSIX_C_SOURCE 200112L

#include "core.h"
#include "util.h"

#include <linux/limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LOOP_SLEEP_100MS (300 * 1000000L)

//--------------------------------------

char g_fo_name_str[32];

//--------------------------------------

bool fo_enabled = true;

int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    pid_t fo_pid = getpid();
    snprintf(g_fo_name_str, sizeof(g_fo_name_str), "Barr FO-%d", fo_pid);

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = LOOP_SLEEP_100MS;

    const char *home = getenv("HOME");
    const char *caller = fo_getcwd();

    char caller_fo_live_log[PATH_MAX];
    snprintf(caller_fo_live_log, sizeof(caller_fo_live_log), "%s/.barr/fo/live_log", caller);

    bool running = true;

    flwrite(caller_fo_live_log, "[%s]: Forward observer deployed by: %s\n", time_str(), caller);
    fo_notify(g_fo_name_str, "Forward Observer deployed", NULL, "dialog-information");

    char caller_cache_path[PATH_MAX];
    snprintf(caller_cache_path, sizeof(caller_cache_path), "%s/.barr/cache/build.cache", caller);
    flappend(caller_fo_live_log, "[%s]: Caller cache file: %s\n", time_str(), caller_cache_path);

    char caller_fo_report[PATH_MAX];
    snprintf(caller_fo_report, sizeof(caller_fo_report), "%s/.barr/fo/report", caller);
    flappend(caller_fo_live_log, "[%s]: Caller report file: %s\n", time_str(), caller_fo_report);

    char caller_fo_lock[PATH_MAX];
    snprintf(caller_fo_lock, sizeof(caller_fo_lock), "%s/.barr/fo/lock", caller);

    char barr_hq_file[PATH_MAX];
    snprintf(barr_hq_file, sizeof(barr_hq_file), "%s/%s/barr_hq", home, BARR_LOCAL_SHARE_DATA_DIR);
    flappend(caller_fo_live_log, "[%s]: Build Barrage HQ file: %s\n", time_str(), barr_hq_file);

    char *barr_root = gtvalue(barr_hq_file, "root");
    flappend(caller_fo_live_log, "[%s]: barr_root: %s\n", time_str(), barr_root);

    char *barr_ver = gtvalue(barr_hq_file, "version");
    flappend(caller_fo_live_log, "[%s]: barr_version: %s\n", time_str(), barr_ver);

    char *barr_build_dir = gtvalue(barr_hq_file, "build_dir");
    flappend(caller_fo_live_log, "[%s]: barr_build_dir: %s\n", time_str(), barr_build_dir);

    char *status = NULL;

    struct Fo_Args args = {.caller_dir = caller, .log_path = caller_fo_live_log, .build_dir = barr_build_dir};

    pthread_t fo_thread;
    pthread_create(&fo_thread, NULL, fo_core_work, &args);

    // fo status loop
    while (running)
    {
        if (!is_file(caller_fo_report))
        {
            flappend(caller_fo_live_log, "[%s]: Shut off - fo report file removed\n", time_str());
            fo_notify(g_fo_name_str, "Report file removed", "low", "system-run");
            running = false;
            fo_enabled = false;
        }

        status = gtvalue(caller_fo_report, "status");

        if (status && strcmp(status, "off") == 0)
        {
            flappend(caller_fo_live_log, "[%s]: fo dismissed\n", time_str());
            fo_notify(g_fo_name_str, "Forward Observer dismissed", NULL, "dialog-information");
            free(status);

            running = false;
            fo_enabled = false;
        }

        nanosleep(&ts, NULL);
    }

    flwrite(caller_fo_report, "status=off\ntimestamp=%ld\n", time(NULL));

    // exit thread
    fo_enabled = false;
    pthread_join(fo_thread, NULL);

    rmrf(caller_fo_lock);

    if (status)
    {
        free(status);
    }
    free(barr_root);
    free(barr_ver);
    free(barr_build_dir);
    return 0;
}
