#include "barr_debug.h"
#include "barr_io.h"

#include <stdlib.h>
#include <sys/wait.h>

barr_i32 BARR_run_process(const char *name, char **args, bool verbose)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        execvp(name, args);
        BARR_errlog("Failed to execvp: %s", name);
        BARR_dumb_backtrace();
        _exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        barr_i32 status;
        if (waitpid(pid, &status, 0) == -1)
        {
            if (verbose)
            {
                BARR_errlog("%s() waitpid failed for %s", __func__, name);
                BARR_dumb_backtrace();
            }
            return -1;
        }

        if (WIFEXITED(status))
        {
            barr_i32 code = WEXITSTATUS(status);
            if (code == 0)
            {
                if (verbose)
                {
                    BARR_log("Success: %s exited with (%d) code", name, code);
                }
                return 0;
            }
            else
            {
                if (verbose)
                {
                    BARR_errlog("Failed: %s exited with (%d) code", name, code);
                }
                return code;
            }
        }
        else if (WIFSIGNALED(status))
        {
            if (verbose)
            {
                BARR_errlog("Failed: %s killed by signal %d", name, WTERMSIG(status));
            }
            return -1;
        }
        else
        {
            if (verbose)
            {
                BARR_errlog("Failed: %s did not exit normally", name);
            }
            return -1;
        }
    }
    else
    {
        BARR_errlog("%s fork failed for %s", __func__, name);
        return -1;
    }
}
