#include "barr_debug.h"
#include "barr_env.h"
#include "barr_gc.h"
#include "barr_io.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

barr_i32 BARR_run_process_dev_null(const char *name, char **args)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        // child process: redirect all output to /dev/null
        int fd = open("/dev/null", O_RDWR);
        if (fd != -1)
        {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        execvp(name, args);
        _exit(127);  // exec failed
    }
    else if (pid > 0)
    {
        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
        {
            return WEXITSTATUS(status);
        }
        return -1;
    }
    else
    {
        return -1;  // fork failed
    }
}

char *BARR_run_process_capture(char *const argv[])
{
    barr_i32 pipefd[2];
    if (pipe(pipefd) == -1)
    {
        return NULL;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execvp(argv[0], argv);
        _exit(127);
    }

    close(pipefd[1]);
    char buffer[BARR_BUF_SIZE_256];
    size_t len = 0;
    size_t cap = BARR_BUF_SIZE_512;
    char *output = BARR_gc_alloc(cap);
    output[0] = '\0';

    ssize_t n;
    while ((n = read(pipefd[0], buffer, sizeof(buffer))) > 0)
    {
        if (len + n + 1 > cap)
        {
            cap *= 2;
            output = BARR_gc_realloc(output, cap);
        }

        memcpy(output + len, buffer, n);
        len += n;
    }

    output[len] = '\0';
    close(pipefd[0]);
    waitpid(pid, NULL, 0);

    if (len > 0 && output[len - 1] == '\n')
    {
        output[len - 1] = '\0';
    }

    return output;
}

barr_i32 BARR_run_process_BG(const char *name, char **args)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        // child process: detach from terminal/session
        if (setsid() == -1)
        {
            BARR_errlog("Failed to setsid for %s", name);
        }

        int fd = open("/dev/null", O_RDWR);
        if (fd != -1)
        {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        execvp(name, args);
        BARR_errlog("Failed to execvp: %s", name);
        _exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        if (g_barr_verbose)
        {
            BARR_log("%s(): Started %s in background with PID %d", __func__, name, pid);
        }
        return pid;  // return child PID so caller can track/kill if needed
    }
    else
    {
        BARR_errlog("%s fork failed for %s", __func__, name);
        return -1;
    }
}

barr_i32 BARR_run_process(const char *name, char **args, bool verbose)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        execvp(name, args);
        BARR_errlog("Failed to execvp: %s", name);
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
