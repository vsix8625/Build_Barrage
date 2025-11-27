#define _POSIX_C_SOURCE 200112L
#define _FILE_OFFSET_BITS 64

#include "core.h"
#include "util.h"

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define FO_IDLE_SEC 5
#define FO_TICK_SLEEP_MS 100

extern bool fo_enabled;
extern char g_fo_name_str[32];

//--------------------------------------

void *fo_core_work(void *arg)
{
    struct Fo_Args *fargs = (struct Fo_Args *) arg;
    fo_watcher_tick(fargs->caller_dir, fargs->log_path, fargs->build_dir);
    return NULL;
}

//--------------------------------------

static time_t get_latest_build_ts(const char *path)
{
    if (path == NULL)
    {
        return 0;
    }

    char *binfo_ts_str = gtvalue(path, "timestamp");
    time_t lbts = 0;
    if (binfo_ts_str)
    {
        lbts = (time_t) strtoll(binfo_ts_str, NULL, 10);
    }

    free(binfo_ts_str);
    return lbts;
}

static bool fo_has_modified_sources(const char *dir, time_t latest_build_ts, time_t last_processed_mtime,
                                    uint64_t *modified, time_t *max_mtime)
{
    if (dir == NULL)
    {
        return false;
    }

    DIR *dp = opendir(dir);

    if (dp == NULL)
    {
        return false;
    }

    static const char *skip_dirs[] = {"build", ".barr", "bin", "obj", ".git", NULL};

    struct dirent *ent;
    while ((ent = readdir(dp)))
    {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
        {
            continue;
        }

        if (ent->d_type == DT_DIR)
        {
            bool skip = false;
            for (const char **p = skip_dirs; *p; ++p)
            {
                if (strcmp(ent->d_name, *p) == 0)
                {
                    skip = true;
                    break;
                }
            }

            if (skip)
            {
                continue;
            }
        }

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

        struct stat st;
        if (stat(path, &st) != 0)
        {
            continue;
        }

        if (S_ISDIR(st.st_mode))
        {
            if (fo_has_modified_sources(path, latest_build_ts, last_processed_mtime, modified, max_mtime))
            {
                closedir(dp);
                return true;
            }
        }
        else if (S_ISREG(st.st_mode))
        {
            const char *ext = strrchr(ent->d_name, '.');
            if (ext == NULL)
            {
                continue;
            }

            if ((strcmp(ext, ".c") == 0 || strcmp(ext, ".cpp") == 0 || strcmp(ext, ".h") == 0 ||
                 strcmp(ext, ".hpp") == 0))
            {
                if (st.st_size == 0)
                {
                    continue;
                }

                time_t now = time(NULL);
                if (st.st_size < 4096 && (now - st.st_mtime) <= 2)
                {
                    continue;
                }

                if (st.st_mtime > latest_build_ts && st.st_mtime > last_processed_mtime)
                {
                    (*modified)++;
                    if (st.st_mtime > *max_mtime)
                    {
                        *max_mtime = st.st_mtime;
                    }

                    closedir(dp);
                    return true;
                }
            }
        }
    }

    closedir(dp);
    return false;
}

void fo_watcher_tick(const char *caller_dir, const char *log_path, const char *build_dir)
{
    flappend(log_path, "[%s]: fo_watcher_tick heartbeat\n", time_str());
    char caller_binfo_path[PATH_MAX];
    snprintf(caller_binfo_path, sizeof(caller_binfo_path), "%s/.barr/data/build_info", caller_dir);

    char caller_fo_wait_file[PATH_MAX];
    snprintf(caller_fo_wait_file, sizeof(caller_fo_wait_file), "%s/.barr/fo/fo_wait.lock", caller_dir);

    time_t latest_real_build_ts = get_latest_build_ts(caller_binfo_path);
    static time_t fo_last_processed_mtime = 0;
    bool build_running = false;
    static uint64_t fail_count = 0;
    static uint64_t modified = 0;
    uint64_t fo_get_ts_retry = 0;  // to gather info from build_info

    char barr_exe[PATH_MAX];
    snprintf(barr_exe, sizeof(barr_exe), "%s/bin/barr", build_dir);

    static time_t last_idle_notify = 0;
    time_t idle_notify_perdiod = 30;

    flappend(log_path, "[%s]: fo_enabled: %d\n", time_str(), fo_enabled);
    while (fo_enabled)
    {
        if (is_file(caller_fo_wait_file))
        {
            build_running = true;

            last_idle_notify = 0;

            flappend(log_path, "[%s]: fo_wait.lock detected at: %s\n", time_str(), caller_dir);
            struct timespec lts = {FO_IDLE_SEC, 0};
            nanosleep(&lts, NULL);

            if (!is_file(caller_fo_wait_file))
            {
                build_running = false;
                flappend(log_path, "[%s]: fo_wait.lock removed - FO resuming\n", time_str());
            }

            continue;
        }

        while ((latest_real_build_ts = get_latest_build_ts(caller_binfo_path)) == 0)
        {
            fo_get_ts_retry++;
            flappend(log_path, "[%s]: build_info not ready, retry %lu\n", time_str(), fo_get_ts_retry);
            if (fo_get_ts_retry % 5 == 0)
            {
                flappend(log_path,
                         "[%s]: build_info not found after %lu retries — did barr build before deploying 'fo'?\n",
                         time_str(), fo_get_ts_retry);
                fo_notify(g_fo_name_str, "Missing build_info — did barr build before deploying 'fo'?", "low",
                          "dialog-warning");
            }

            struct timespec ts = {4, 0};
            nanosleep(&ts, NULL);
        }

        if (latest_real_build_ts == 0)
        {
            flappend(log_path, "[%s]: no valid build timestamp after retries, stopping FO\n", time_str());
            fo_enabled = false;
            break;
        }

        time_t now = time(NULL);

        if (!build_running && (now - latest_real_build_ts) >= FO_IDLE_SEC)
        {
            if (now - last_idle_notify >= idle_notify_perdiod)
            {
                flappend(log_path, "[%s]: idle — no changes detected\n", time_str());
                fo_notify(g_fo_name_str, "Idle: watching for changes…", "low", "eye");
                last_idle_notify = now;
            }

            time_t max_mtime = fo_last_processed_mtime;
            modified = 0;

            if (fo_has_modified_sources(caller_dir, latest_real_build_ts, fo_last_processed_mtime, &modified,
                                        &max_mtime) &&
                modified > 0)
            {
                build_running = true;
                fo_last_processed_mtime = max_mtime;

                last_idle_notify = 0;

                flappend(log_path, "[%s]: TRIGGERED — modified sources detected, ts %ld vs latest %ld\n", time_str(),
                         time(NULL), latest_real_build_ts);
                fo_notify(g_fo_name_str, "Rebuild triggered...", "low", "system-run");

                char *args[] = {barr_exe, "build", "--dir", (char *) caller_dir, NULL};
                int ret = run_process_blocking(args[0], args);

                if (ret != 0)
                {
                    fail_count++;
                    time_t backoff = FO_IDLE_SEC * fail_count;

                    fo_notify(g_fo_name_str, "Build FAILED", "critical", "dialog-error");
                    flappend(log_path, "[%s]: build failed, exit %d, backoff %ld sec (fail# %lu)\n", time_str(), ret,
                             backoff, fail_count);

                    if (fail_count > 10)
                    {
                        fo_enabled = false;
                        flappend(log_path, "[%s]: FO disabled after %lu consecutive failures\n", time_str(),
                                 fail_count);
                        fo_notify(g_fo_name_str, "Disabled after repeated failures", "critical", "dialog-error");
                        break;
                    }

                    struct timespec fts = {backoff, 0};
                    nanosleep(&fts, NULL);
                }
                else
                {
                    fail_count = 0;

                    latest_real_build_ts = get_latest_build_ts(caller_binfo_path);

                    fo_notify(g_fo_name_str, "Build successful", NULL, "checkbox-checked");
                    flappend(log_path, "[%s]: barr build exit: %d: %s\n", time_str(), ret, caller_dir);

                    flappend(log_path, "[%s]: latest_real_build_ts: %ld \n", time_str(), latest_real_build_ts);
                    flappend(log_path, "[%s]: fo_last_processed_mtime: %ld \n", time_str(), fo_last_processed_mtime);
                    flappend(log_path, "[%s]: now: %ld \n", time_str(), now);
                }
            }
            build_running = false;
        }
        struct timespec ts = {0, FO_TICK_SLEEP_MS * 1000000L};
        nanosleep(&ts, NULL);
    }
}
