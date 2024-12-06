#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>

#define SHM_NAME_1 "/shm112"
#define SHM_NAME_2 "/shm222"

#define SEM_WRITE_1 "/sw112"
#define SEM_WRITE_2 "/sw222"

#define SEM_READ_1 "/sr112"
#define SEM_READ_2 "/sr222"

#define BUFFER_SIZE 1024