#include <stdint.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>

static char CLIENT_PROGRAM_NAME[] = "client";

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        char msg[1024];
        uint32_t len = snprintf(msg, sizeof(msg) - 1, "usage: %s filename\n", argv[0]);
        write(STDERR_FILENO, msg, len);
        exit(EXIT_SUCCESS);
    }

    char progpath[1024];
    {
        ssize_t len = readlink("/proc/self/exe", progpath,
                               sizeof(progpath) - 1);
        if (len == -1)
        {
            const char msg[] = "error: failed to read full program path\n";
            write(STDERR_FILENO, msg, sizeof(msg));
            exit(EXIT_FAILURE);
        }

        while (progpath[len] != '/')
            --len;

        progpath[len] = '\0';
    }

    int channel_1[2], channel_2[2];
    if (pipe(channel_1) == -1)
    {
        const char msg[] = "error: failed to create pipe\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }
    if (pipe(channel_2) == -1)
    {
        const char msg[] = "error: failed to create pipe\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }

    const pid_t child_1 = fork();

    switch (child_1)
    {
    case -1:
    {
        const char msg[] = "error: failed to spawn new process\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }
    break;

    case 0:
    {
        pid_t pid = getpid();

        if (dup2(channel_1[STDIN_FILENO], STDIN_FILENO) == -1)
        {
            const char msg[128];
            snprintf(msg, sizeof(msg), "%d: failed to use dup2\n", pid);
            write(STDERR_FILENO, msg, sizeof(msg));
            exit(EXIT_FAILURE);
        }
        if (close(channel_1[STDOUT_FILENO]) == -1)
        {
            const char msg[128];
            snprintf(msg, sizeof(msg), "%d: failed to close pipe\n", pid);
            write(STDERR_FILENO, msg, sizeof(msg));
            exit(EXIT_FAILURE);
        }

        {
            char msg[64];
            const int32_t length = snprintf(msg, sizeof(msg),
                                            "%d: I'm a child1\n", pid);
            write(STDOUT_FILENO, msg, length);
        }

        {
            char path[1024];
            snprintf(path, sizeof(path) - 1, "%s/%s", progpath, CLIENT_PROGRAM_NAME);

            char *const args[] = {CLIENT_PROGRAM_NAME, argv[1], NULL};

            int32_t status = execv(path, args);

            if (status == -1)
            {
                const char msg[] = "error: failed to exec into new exectuable image\n";
                write(STDERR_FILENO, msg, sizeof(msg));
                exit(EXIT_FAILURE);
            }
        }
    }
    break;

    default:
    {
        const pid_t child_2 = fork();

        switch (child_2)
        {
        case -1:
        {
            const char msg[] = "error: failed to spawn new process\n";
            write(STDERR_FILENO, msg, sizeof(msg));
            exit(EXIT_FAILURE);
        }
        break;

        case 0:
        {
            pid_t pid = getpid();

            if (dup2(channel_2[STDIN_FILENO], STDIN_FILENO) == -1)
            {
                const char msg[128];
                snprintf(msg, sizeof(msg), "%d: failed to use dup2\n", pid);
                write(STDERR_FILENO, msg, sizeof(msg));
                exit(EXIT_FAILURE);
            }
            if (close(channel_2[STDOUT_FILENO]) == -1)
            {
                const char msg[128];
                snprintf(msg, sizeof(msg), "%d: failed to close pipe\n", pid);
                write(STDERR_FILENO, msg, sizeof(msg));
                exit(EXIT_FAILURE);
            }

            {
                char msg[64];
                const int32_t length = snprintf(msg, sizeof(msg),
                                                "%d: I'm a child2\n", pid);
                write(STDOUT_FILENO, msg, length);
            }

            {
                char path[1024];
                snprintf(path, sizeof(path) - 1, "%s/%s", progpath, CLIENT_PROGRAM_NAME);

                char *const args[] = {CLIENT_PROGRAM_NAME, argv[2], NULL};

                int32_t status = execv(path, args);

                if (status == -1)
                {
                    const char msg[] = "error: failed to exec into new exectuable image\n";
                    write(STDERR_FILENO, msg, sizeof(msg));
                    exit(EXIT_FAILURE);
                }
            }
        }
        break;

        default:
        {
            pid_t pid = getpid();

            {
                char msg[128];
                const int32_t length = snprintf(msg, sizeof(msg),
                                                "%d: I'm a parent, my child1 & child2 has PID %d %d\n", pid, child_1, child_2);
                write(STDOUT_FILENO, msg, length);
            }
            if (close(channel_1[STDIN_FILENO]) == -1 || close(channel_2[STDIN_FILENO]) == -1)
            {
                const char msg[] = "error: server failed to close pipe\n";
                write(STDERR_FILENO, msg, sizeof(msg));
                exit(EXIT_FAILURE);
            }

            char buf[4096];
            ssize_t bytes;
            int odd = 1;
            {
                const char msg[] = "Input strings:\n";
                write(STDOUT_FILENO, msg, sizeof(msg));
            }
            while (bytes = read(STDIN_FILENO, buf, sizeof(buf)))
            {
                if (bytes < 0)
                {
                    const char msg[] = "error: failed to read from stdin\n";
                    write(STDERR_FILENO, msg, sizeof(msg));
                    exit(EXIT_FAILURE);
                }
                else if (buf[0] == '\n')
                {
                    break;
                }
                {
                    buf[bytes - 1] = '\0';
                    int32_t written;
                    if (odd)
                        written = write(channel_1[STDOUT_FILENO], buf, bytes);
                    else
                        written = write(channel_2[STDOUT_FILENO], buf, bytes);
                    odd = abs(odd - 1);
                    if (written != bytes)
                    {
                        const char msg[] = "error: client failed to write to file\n";
                        write(STDERR_FILENO, msg, sizeof(msg));
                        exit(EXIT_FAILURE);
                    }
                }
            }
            buf[0] = '\n';
            if (write(channel_1[STDOUT_FILENO], buf, sizeof(char)) == -1 || write(channel_2[STDOUT_FILENO], buf, sizeof(char)) == -1)
            {
                const char msg[] = "error: server failed to write to file\n";
                write(STDERR_FILENO, msg, sizeof(msg));
                exit(EXIT_FAILURE);
            }
            if (close(channel_1[STDOUT_FILENO]) == -1 || close(channel_2[STDOUT_FILENO] == -1))
            {
                const char msg[] = "error: server failed to close pipe\n";
                write(STDERR_FILENO, msg, sizeof(msg));
                exit(EXIT_FAILURE);
            }

            // NOTE: `wait` blocks the parent until childs exits
            int child_status;
            pid_t wpid;
            while ((wpid = wait(&child_status) > 0))
            {
                if (child_status != EXIT_SUCCESS)
                {
                    const char msg[] = "error: child exited with error\n";
                    write(STDERR_FILENO, msg, sizeof(msg));
                    exit(child_status);
                }
            }
        }
        break;
        }
        break;
    }
    }
    return 0;
}