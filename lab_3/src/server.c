#include "lib.h"

static char CLIENT_PROGRAM_NAME[] = "client";

int main(int argc, char **argv)
{
    int shm_fd1, shm_fd2;
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

    if ((shm_fd1 = shm_open(SHM_NAME_1, O_CREAT | O_RDWR, 0666)) == -1)
    {
        const char msg[] = "error: failed to open shared memory 1";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }
    if (ftruncate(shm_fd1, BUFFER_SIZE) == -1)
    {
        const char msg[] = "error: failed to set size for shared memory 1";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }
    if ((shm_fd2 = shm_open(SHM_NAME_2, O_CREAT | O_RDWR, 0666)) == -1)
    {
        const char msg[] = "error: failed to open shared memory 2";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }
    if (ftruncate(shm_fd2, BUFFER_SIZE) == -1)
    {
        const char msg[] = "error: failed to set size for shared memory 1";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }

    char *shared_memory1 = mmap(0, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd1, 0);
    char *shared_memory2 = mmap(0, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd2, 0);
    if (shared_memory1 == MAP_FAILED || shared_memory2 == MAP_FAILED)
    {
        const char msg[] = "error: failed to map shared memory";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }

    sem_t *sem_write1 = sem_open(SEM_WRITE_1, O_CREAT | O_EXCL, 0644, 1);
    sem_t *sem_read1 = sem_open(SEM_READ_1, O_CREAT | O_EXCL, 0644, 0);

    sem_t *sem_write2 = sem_open(SEM_WRITE_2, O_CREAT | O_EXCL, 0644, 1);
    sem_t *sem_read2 = sem_open(SEM_READ_2, O_CREAT | O_EXCL, 0644, 0);

    if (sem_write1 == SEM_FAILED || sem_read1 == SEM_FAILED || sem_write1 == SEM_FAILED || sem_write2 == SEM_FAILED)
    {
        const char msg[] = "error: failed to open semaphore";
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

        {
            char msg[64];
            const int32_t length = snprintf(msg, sizeof(msg),
                                            "%d: I'm a child1\n", pid);
            write(STDOUT_FILENO, msg, length);
        }

        {
            char path[1024];
            snprintf(path, sizeof(path) - 1, "%s/%s", progpath, CLIENT_PROGRAM_NAME);

            char *const args[] = {CLIENT_PROGRAM_NAME, argv[1], SEM_WRITE_1, SEM_READ_1, SHM_NAME_1, NULL};

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

            {
                char msg[64];
                const int32_t length = snprintf(msg, sizeof(msg),
                                                "%d: I'm a child2\n", pid);
                write(STDOUT_FILENO, msg, length);
            }

            {
                char path[1024];
                snprintf(path, sizeof(path) - 1, "%s/%s", progpath, CLIENT_PROGRAM_NAME);

                char *const args[] = {CLIENT_PROGRAM_NAME, argv[2], SEM_WRITE_2, SEM_READ_2, SHM_NAME_2, NULL};
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

            char buf[BUFFER_SIZE];
            ssize_t bytes;
            int odd = 1;
            {
                sleep(1);
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
                    {
                        sem_wait(sem_write1);
                        snprintf(shared_memory1, BUFFER_SIZE, buf);
                        sem_post(sem_read1);
                    }

                    else
                    {
                        sem_wait(sem_write2);
                        snprintf(shared_memory2, BUFFER_SIZE, buf);
                        sem_post(sem_read2);
                    }

                    odd = abs(odd - 1);
                }
            }
            buf[0] = '\n';
            sem_wait(sem_write1);
            snprintf(shared_memory1, BUFFER_SIZE, buf);
            sem_post(sem_read1);
            sem_wait(sem_write2);
            snprintf(shared_memory2, BUFFER_SIZE, buf);
            sem_post(sem_read2);

            if (sem_close(sem_write1) == -1 ||
                sem_close(sem_write2) == -1 ||
                sem_close(sem_read1) == -1 ||
                sem_close(sem_read2) == -1)
            {
                const char msg[] = "error: failed to sem_close";
                write(STDERR_FILENO, msg, sizeof(msg));
                exit(EXIT_FAILURE);
            }

            if (sem_unlink(SEM_WRITE_1) == -1 ||
                sem_unlink(SEM_WRITE_2) == -1 ||
                sem_unlink(SEM_READ_1) == -1 ||
                sem_unlink(SEM_READ_2) == -1)
            {
                const char msg[] = "error: failed to sem_unlink";
                write(STDERR_FILENO, msg, sizeof(msg));
                exit(EXIT_FAILURE);
            }

            if (munmap(shared_memory1, BUFFER_SIZE) || munmap(shared_memory2, BUFFER_SIZE))
            {
                const char msg[] = "error: failed to munmap";
                write(STDERR_FILENO, msg, sizeof(msg));
                exit(EXIT_FAILURE);
            }

            if (shm_unlink(SHM_NAME_1) == -1 || shm_unlink(SHM_NAME_2) == -1)
            {
                const char msg[] = "error: failed to shm_unlink";
                write(STDERR_FILENO, msg, sizeof(msg));
                exit(EXIT_FAILURE);
            }

            int child_status;
            pid_t wpid;
            while ((wpid = wait(&child_status)) > 0)
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