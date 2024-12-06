#include "lib.h"
#include "string.h"

void str_reverse(char *str)
{
    int len = strlen(str);
    for (int i = 0; i < len / 2; ++i)
    {
        char temp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = temp;
    }
}

int main(int argc, char **argv)
{
    char buf[BUFFER_SIZE];
    ssize_t bytes;

    pid_t pid = getpid();

    // NOTE: `O_WRONLY` only enables file for writing
    // NOTE: `O_CREAT` creates the requested file if absent
    // NOTE: `O_TRUNC` empties the file prior to opening
    // NOTE: `O_APPEND` subsequent writes are being appended instead of overwritten
    int32_t file = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
    if (file == -1)
    {
        const char msg[] = "error: failed to open requested file\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }

    int shm_fd = shm_open(argv[4], O_RDONLY, 0666);
    if (shm_fd == -1)
    {
        const char msg[] = "error: failed to open shared memory";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }
    char *shared_memory = mmap(0, BUFFER_SIZE, PROT_READ, MAP_SHARED, shm_fd, 0);
    if (shared_memory == MAP_FAILED)
    {
        const char msg[] = "error: failed to map shared memory";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }

    sem_t *sem_write = sem_open(argv[2], 0);
    sem_t *sem_read = sem_open(argv[3], 0);
    if (sem_write == SEM_FAILED || sem_read == SEM_FAILED)
    {
        const char msg[] = "error: client failed to open semaphore";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }

    char flag = 1;
    do
    {
        sem_wait(sem_read);
        if (*shared_memory == '\n')
        {
            flag = 0;
        }
        else
        {

            bytes = strlen(shared_memory) + 1;
            strcpy(buf, shared_memory);

            sem_post(sem_write);

            buf[bytes - 1] = '\0';
            str_reverse(buf);
            buf[bytes - 1] = '\n';

            int32_t written = write(file, buf, bytes);
            if (written != bytes)
            {
                const char msg[] = "error: client failed to write to file\n";
                write(STDERR_FILENO, msg, sizeof(msg));
                exit(EXIT_FAILURE);
            }
        }
    } while (flag);

    if (close(file) == -1)
    {
        const char msg[] = "error: client failed to close file\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }
    if (sem_close(sem_write) == -1 || sem_close(sem_read) == -1)
    {
        const char msg[] = "error: client failed to sem_close\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }

    if (munmap(shared_memory, BUFFER_SIZE) == -1)
    {
        const char msg[] = "error: client failed to munmap";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }
    return 0;
}