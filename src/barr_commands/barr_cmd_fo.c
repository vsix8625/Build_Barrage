#include "barr_cmd_fo.h"
#include "barr_cmd_version.h"
#include "barr_env.h"
#include "barr_gc.h"
#include "barr_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void barr_fo_on(void)
{
    time_t tstamp = time(NULL);

    if (!BARR_isdir(BARR_FO_DIR))
    {
        BARR_mkdir_p(BARR_FO_DIR);
    }

    BARR_file_write(BARR_FO_REPORT_FILE, "status=on\ntimestamp=%ld\n", tstamp);
    BARR_file_write(BARR_FO_LOCK, "%ld\n", tstamp);
}

static void barr_fo_off(void)
{
    time_t tstamp = time(NULL);

    if (!BARR_isdir(BARR_FO_DIR) || !BARR_isfile(BARR_FO_REPORT_FILE))
    {
        return;
    }

    BARR_file_write(BARR_FO_REPORT_FILE, "status=off\ntimestamp=%ld\n", tstamp);
    if (BARR_isfile(BARR_FO_LOCK))
    {
        BARR_rmrf(BARR_FO_LOCK);
    }
}

barr_i32 BARR_command_fo(barr_i32 argc, char **argv)
{
    if (argc == 1)
    {
        BARR_printf("Usage: %s <opts>\n", argv[0]);
        if (BARR_isfile(BARR_FO_REPORT_FILE))
        {
            BARR_printf("%s", BARR_file_read(BARR_FO_REPORT_FILE));
        }
        return 0;
    }

    if (!BARR_init())
    {
        return 1;
    }

    for (barr_i32 i = 1; i < argc; ++i)
    {
        const char *opt = argv[i];

        if (BARR_strmatch(opt, "deploy") || BARR_strmatch(opt, "-d"))
        {
            if (BARR_isfile(BARR_FO_LOCK))
            {
                BARR_errlog("Forward observer is already deployed for: %s", BARR_getcwd());
                return 1;
            }
            barr_fo_on();

            char barr_hq_file[BARR_PATH_MAX];
            snprintf(barr_hq_file, sizeof(barr_hq_file), "%s/%s", BARR_GET_HOME(), BARR_LOCAL_SHARE_BARR_HQ);

            const char *root = BARR_get_value(barr_hq_file, "root");

            if (root == NULL)
            {
                BARR_errlog("%s(): failed to get barr root dir", __func__);
                return 1;
            }

            // TODO: move this to .local/share
            char fo_barr_binfo[BARR_PATH_MAX];
            snprintf(fo_barr_binfo, sizeof(fo_barr_binfo), "%s/tools/fo/.barr/data/build_info", root);

            char *fo_build_dir = BARR_get_build_info_key(fo_barr_binfo, "build_dir");
            char *fo_name = BARR_get_build_info_key(fo_barr_binfo, "name");
            char exe_path[BARR_PATH_MAX];
            snprintf(exe_path, sizeof(exe_path), "%s/bin/%s", fo_build_dir, fo_name);

            char *args[] = {exe_path, NULL};
            pid_t pid = BARR_run_process_BG(args[0], args);
            BARR_log("Forward Observer deployed: %d", pid);
            return (pid > 0) ? 0 : pid;
        }

        if (BARR_strmatch(opt, "dismiss") || BARR_strmatch(opt, "-s"))
        {
            barr_fo_off();
            BARR_log("Forward observer dismissed");
            return 0;
        }

        if (BARR_strmatch(opt, "report"))
        {
            BARR_printf("%s", BARR_file_read(BARR_FO_REPORT_FILE));
            return 0;
        }

        if (BARR_strmatch(opt, "watch") || BARR_strmatch(opt, "-w"))
        {
            char cmd[BARR_PATH_MAX + 128];
            snprintf(cmd, sizeof(cmd), "watch -n 1 'tail -n 50 %s | bat --style=plain -l text'", BARR_FO_LOG_FILE);

            char *args[] = {"sh", "-c", cmd, NULL};
            return BARR_run_process(args[0], args, false);
        }
    }

    return 0;
}

char *BARR_get_value(const char *fpath, const char *key)
{
    if (fpath == NULL || key == NULL)
    {
        return NULL;
    }

    FILE *fp = fopen(fpath, "r");
    if (fp == NULL)
    {
        return NULL;
    }

    size_t key_len = strlen(key);
    char line[BARR_PATH_MAX];

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char *p = line;

        /* trim leading whitespace */
        while (*p == ' ' || *p == '\t')
        {
            p++;
        }

        /* skip empty/comment lines */
        if (*p == '\0' || *p == '\n' || *p == '#')
        {
            continue;
        }

        /* check key match */
        if (strncmp(p, key, key_len) == 0)
        {
            char *q = p + key_len;

            /* skip spaces between key and = */
            while (*q == ' ' || *q == '\t')
            {
                q++;
            }

            /* now expect '=' */
            if (*q != '=')
            {
                continue;
            }

            q++; /* skip '=' */

            /* skip whitespace before value */
            while (*q == ' ' || *q == '\t')
            {
                q++;
            }

            /* trim trailing newline */
            char *nl = strchr(q, '\n');
            if (nl != NULL)
            {
                *nl = '\0';
            }

            fclose(fp);
            return BARR_gc_strdup(q);
        }
    }

    fclose(fp);
    return NULL;
}
