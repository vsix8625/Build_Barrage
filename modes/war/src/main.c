#include <libgen.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

volatile sig_atomic_t running = 1;

void handle_sigterm(int sig)
{
    (void) sig;
    running = 0;
}

#define PLAYER "paplay"
#define WAR_MODE_REL_ASSETS_PATH "assets"

static void get_assets_path(char *out, size_t out_size)
{
    char exec_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exec_path, sizeof(exec_path) - 1);

    if (len == -1)
    {
        snprintf(out, out_size, WAR_MODE_REL_ASSETS_PATH);
        return;
    }

    exec_path[len] = '\0';
    char *dir = dirname(exec_path);

    /* Move one directory up */
    char parent_dir[PATH_MAX];
    snprintf(parent_dir, sizeof(parent_dir), "%s/..", dir);

    /* Normalize path (optional, realpath handles ../) */
    char resolved_parent[PATH_MAX];
    if (realpath(parent_dir, resolved_parent) == NULL)
    {
        /* If realpath fails, fallback to relative path */
        snprintf(out, out_size, "%s/%s", parent_dir, WAR_MODE_REL_ASSETS_PATH);
        return;
    }

    snprintf(out, out_size, "%s/%s", resolved_parent, WAR_MODE_REL_ASSETS_PATH);
}

static void spawn_sound(const char *sound)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        char *args[] = {PLAYER, (char *) sound, NULL};
        execvp(PLAYER, args);
        _exit(EXIT_FAILURE);
    }
}

#define BARR_LOCAL_SHARE_MODES_ACTIVE_FILE ".local/share/barr/modes/active"

static bool is_mode_still_active(const char *expected)
{
    char *home = getenv("HOME");
    char fullpath[PATH_MAX];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", home, BARR_LOCAL_SHARE_MODES_ACTIVE_FILE);

    FILE *f = fopen(fullpath, "r");
    if (!f)
    {
        return false;
    }

    char buf[64];
    if (!fgets(buf, sizeof(buf), f))
    {
        fclose(f);
        return false;
    }
    fclose(f);

    buf[strcspn(buf, "\n")] = '\0';
    return strcmp(buf, expected) == 0;
}

int main(void)
{
    signal(SIGTERM, handle_sigterm);
    srand(time(NULL));

    char assets_dir[PATH_MAX];
    get_assets_path(assets_dir, sizeof(assets_dir));
    //    printf("[WAR-MODE]: assets path = %s\n", assets_dir);

    pid_t base_pid = fork();
    if (base_pid == 0)
    {
        sleep(2);

        while (running && is_mode_still_active("war"))
        {
            char warfare_sound_path[PATH_MAX + 32];
            snprintf(warfare_sound_path, sizeof(warfare_sound_path), "%s/distant-warfare.mp3", assets_dir);
            spawn_sound(warfare_sound_path);
            for (int i = 0; i < 360; i += 5)
            {
                if (!running || !is_mode_still_active("war"))
                {
                    kill(base_pid, SIGTERM);
                    waitpid(base_pid, NULL, 0);
                    break;
                }
                sleep(5);
            }
        }
        _exit(EXIT_SUCCESS);
    }

    while (running && is_mode_still_active("war"))
    {
        sleep(5);
    }

    kill(base_pid, SIGTERM);
    waitpid(base_pid, NULL, 0);
    return 0;
}
