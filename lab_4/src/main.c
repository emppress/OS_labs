#include "allocator.h"

#include <unistd.h>
#include <dlfcn.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#define MEM_SIZE 9999999999

static size_t __USE_MEMORY = 0;

static allocator_alloc_f *allocator_alloc;
static allocator_create_f *allocator_create;
static allocator_destroy_f *allocator_destroy;
static allocator_free_f *allocator_free;
static get_used_memory_f *get_used_memory;

typedef struct fallback_allocator
{
    size_t size;
    void *memory;
} fallback_allocator;

static Allocator *fallback_allocator_create(void *const memory, const size_t size)
{
    if (memory == NULL || size <= sizeof(fallback_allocator))
    {
        return NULL;
    }
    fallback_allocator *alloc = (fallback_allocator *)((uint8_t *)memory);
    alloc->memory = memory + sizeof(fallback_allocator);
    alloc->size = size - sizeof(fallback_allocator);

    __USE_MEMORY = sizeof(fallback_allocator);

    return (Allocator *)alloc;
}

static void fallback_allocator_destroy(Allocator *const allocator)
{
    if (allocator == NULL)
        return;

    ((fallback_allocator *)allocator)->memory = NULL;
    ((fallback_allocator *)allocator)->size = 0;
}

static void *fallback_allocator_alloc(Allocator *const allocator, const size_t size)
{
    if (allocator == NULL || size == 0)
    {
        return NULL;
    }

    static size_t offset = 0;

    fallback_allocator *all = (fallback_allocator *)allocator;
    if (offset + size > ((fallback_allocator *)allocator)->size)
    {
        return NULL;
    }

    void *allocated_memory = (void *)((char *)((fallback_allocator *)allocator)->memory + offset);
    offset += size;

    __USE_MEMORY += size;

    return allocated_memory;
}

static void fallback_allocator_free(Allocator *const allocator, void *const memory)
{
    (void)allocator;
    (void)memory;
}

static size_t fallback_allocator_get_used_memory()
{
    return __USE_MEMORY;
}

void test_edge_cases(Allocator *allocator, size_t total_size)
{
    // Тест на исчерпание памяти
    size_t alloc_size = total_size / 2;
    void *ptr1 = allocator_alloc(allocator, alloc_size);
    if (ptr1 == NULL)
    {
        char msg[] = "test_edge_cases: memory allocation error\n";
        write(STDOUT_FILENO, msg, sizeof(msg));
    }

    void *ptr2 = allocator_alloc(allocator, alloc_size + 1);
    if (ptr2 != NULL)
    {
        char msg[] = "test_edge_cases: allocating more memory than the available size\n";
        write(STDOUT_FILENO, msg, sizeof(msg));
        allocator_free(allocator, ptr2);
    }

    allocator_free(allocator, ptr1);

    {
        char msg[] = "test_edge_cases - OK\n";
        write(STDOUT_FILENO, msg, sizeof(msg));
        allocator_free(allocator, ptr2);
    }
}

void test_fragmentation_list(Allocator *allocator, size_t total_size)
{
    if (allocator == NULL)
        return;
    // Выделим и освободим несколько блоков в случайном порядке, чтобы создать фрагментацию
    void *ptrs[1000];
    for (int i = 0; i < 1000; i++)
    {
        ptrs[i] = allocator_alloc(allocator, 100 + i * 10);
        if (ptrs[i] == NULL)
        {
            for (int j = 0; j < i; j++)
            {
                allocator_free(allocator, ptrs[i]);
            }
            char msg[] = "test_fragmentation_list - BAD\n";
            write(STDOUT_FILENO, msg, sizeof(msg));
            return;
        }
    }
    for (int i = 0; i < 1000; i += 2)
    {
        allocator_free(allocator, ptrs[i]);
    }
    for (int i = 1; i < 1000; i += 2)
    {
        allocator_free(allocator, ptrs[i]);
    }

    void *large_ptr = allocator_alloc(allocator, total_size / 2);
    if (large_ptr == NULL)
    {
        char msg[] = "test_fragmentation_list - BAD\n";
        write(STDOUT_FILENO, msg, sizeof(msg));
        return;
    }

    allocator_free(allocator, large_ptr);
    char msg[] = "test_fragmentation_list - OK\n";
    write(STDOUT_FILENO, msg, sizeof(msg));
}

void test_reuse_blocks(Allocator *allocator, size_t total_size)
{
    if (allocator == NULL)
        return;
    // Выделяем блок
    void *ptr1 = allocator_alloc(allocator, 128);
    if (ptr1 == NULL)
    {
        char msg[] = "test_reuse_blocks: error\n";
        write(STDOUT_FILENO, msg, sizeof(msg));
        return;
    }
    // Освобождаем блок
    allocator_free(allocator, ptr1);
    // Выделяем снова блок такого же размера
    void *ptr2 = allocator_alloc(allocator, 128);
    if (ptr2 == NULL)
    {
        char msg[] = "test_reuse_blocks: error\n";
        write(STDOUT_FILENO, msg, sizeof(msg));
        return;
    }

    // Проверяем, что блоки повторно используются
    if (ptr1 == ptr2)
    {
        char msg[] = "test_reuse_blocks - Reused same block +\n";
        write(STDOUT_FILENO, msg, sizeof(msg));
    }
    else
    {
        char msg[] = "test_reuse_blocks - Reused diff block -\n";
        write(STDOUT_FILENO, msg, sizeof(msg));
    }
    allocator_free(allocator, ptr2);
}

void test_alloc_and_free_time(Allocator *allocator)
{
    clock_t start, stop;
    double cpu_time_used;
    if (!allocator)
        return;

    void *ptrs[100000];

    start = clock();
    for (int i = 0; i < 100000; i++)
    {
        ptrs[i] = allocator_alloc(allocator, (10 + i) % 200 + 1);
        if (ptrs[i] == NULL)
        {
            char msg[] = "test_alloc_and_free_time: error\n";
            write(STDOUT_FILENO, msg, sizeof(msg));
            return;
        }
    }
    stop = clock();
    cpu_time_used = ((double)(stop - start)) / CLOCKS_PER_SEC;
    {
        char msg[200];
        sprintf(msg, "alloc time: %f seconds\n", cpu_time_used);
        write(STDOUT_FILENO, msg, strlen(msg));
    }

    start = clock();
    for (int i = 0; i < 100000; i++)
    {
        allocator_free(allocator, ptrs[i]);
        if (ptrs[i] == NULL)
        {
            char msg[] = "test_alloc_and_free_time: error\n";
            write(STDOUT_FILENO, msg, sizeof(msg));
            return;
        }
    }
    stop = clock();
    cpu_time_used = ((double)(stop - start)) / CLOCKS_PER_SEC;
    {
        char msg[200];
        sprintf(msg, "free time: %f seconds\n", cpu_time_used);
        write(STDOUT_FILENO, msg, strlen(msg));
    }

    for (int i = 0; i < 100000; i++)
    {
        ptrs[i] = allocator_alloc(allocator, (10 + i) % 100 + 1);
        if (ptrs[i] == NULL)
        {
            char msg[] = "test_alloc_and_free_time: error\n";
            write(STDOUT_FILENO, msg, sizeof(msg));
            return;
        }
    }
    start = clock();
    for (int i = 0; i < 100000; i += 2)
    {
        allocator_free(allocator, ptrs[i]);
    }
    stop = clock();
    cpu_time_used = ((double)(stop - start)) / CLOCKS_PER_SEC;
    {
        char msg[200];
        sprintf(msg, "free time (with segmentation): %f seconds\n", cpu_time_used);
        write(STDOUT_FILENO, msg, strlen(msg));
    }

    start = clock();
    for (int i = 0; i < 100000; i += 2)
    {
        ptrs[i] = allocator_alloc(allocator, (10 + i) % 300 + 1);
        if (ptrs[i] == NULL)
        {
            char msg[] = "test_alloc_and_free_time: error\n";
            write(STDOUT_FILENO, msg, sizeof(msg));
            return;
        }
    }
    stop = clock();
    cpu_time_used = ((double)(stop - start)) / CLOCKS_PER_SEC;
    {
        char msg[200];
        sprintf(msg, "alloc time (with segmentation): %f seconds\n", cpu_time_used);
        write(STDOUT_FILENO, msg, strlen(msg));
    }

    for (int i = 0; i < 100000; i++)
    {
        allocator_free(allocator, ptrs[i]);
    }
}

int main(int argc, char *argv[])
{
    size_t _MEM_USED = 0;
    void *library = NULL;

    if (argc == 1)
    {
        char msg[] = "The path to the library has not been transmitted, enabling backup functions.\n";
        write(STDOUT_FILENO, msg, sizeof(msg));

        allocator_create = fallback_allocator_create;
        allocator_destroy = fallback_allocator_destroy;
        allocator_alloc = fallback_allocator_alloc;
        allocator_free = fallback_allocator_free;
        get_used_memory = fallback_allocator_get_used_memory;
    }
    else
    {
        library = dlopen(argv[1], RTLD_LOCAL | RTLD_NOW);
        {
            char msg[] = "Trying to open the library.\n";
            write(STDOUT_FILENO, msg, sizeof(msg));
        }
        if (library)
        {
            {
                char msg[] = "Loading functions from the library.\n";
                write(STDOUT_FILENO, msg, sizeof(msg));
            }
            allocator_create = dlsym(library, "allocator_create");
            allocator_destroy = dlsym(library, "allocator_destroy");
            allocator_alloc = dlsym(library, "allocator_alloc");
            allocator_free = dlsym(library, "allocator_free");
            get_used_memory = dlsym(library, "get_used_memory");
        }
        if (!library || !allocator_create || !allocator_destroy || !allocator_alloc || !allocator_free || !get_used_memory)
        {
            {
                char msg[] = "Enabling backup functions.\n";
                write(STDOUT_FILENO, msg, sizeof(msg));
            }
            allocator_create = fallback_allocator_create;
            allocator_destroy = fallback_allocator_destroy;
            allocator_alloc = fallback_allocator_alloc;
            allocator_free = fallback_allocator_free;
            get_used_memory = fallback_allocator_get_used_memory;
        }
    }

    size_t memory_size = MEM_SIZE;
    void *memory = mmap(NULL, memory_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED)
    {
        char msg[] = "mmap failed\n";
        write(STDOUT_FILENO, msg, sizeof(msg));
        if (library)
            dlclose(library);
        return EXIT_FAILURE;
    }

    Allocator *allocator = allocator_create(memory, memory_size);
    if (allocator == NULL)
    {
        char msg[] = "allocator_create failed\n";
        write(STDOUT_FILENO, msg, sizeof(msg));
        munmap(memory, memory_size);
        if (library)
            dlclose(library);
        return EXIT_FAILURE;
    }

    // удобство использования аллокатора на базовой задаче и фактор использования
    {

        int rows = 20, cols = 20;
        int **matrix;
        int num = 0;

        // Выделение памяти под строки матрицы
        matrix = (int **)allocator_alloc(allocator, rows * sizeof(int *));

        _MEM_USED += rows * sizeof(int *);

        if (matrix == NULL)
        {
            char msg[] = "allocator_alloc failed\n";
            write(STDOUT_FILENO, msg, sizeof(msg));
            munmap(memory, memory_size);
            if (library)
                dlclose(library);
            return EXIT_FAILURE;
        }

        // Выделение памяти под столбцы для каждой строки
        for (int i = 0; i < rows; i++)
        {
            matrix[i] = (int *)allocator_alloc(allocator, cols * sizeof(int));

            _MEM_USED += cols * sizeof(int);

            if (matrix[i] == NULL)
            {
                for (int j = 0; j < i; j++)
                {
                    allocator_free(allocator, matrix[j]);
                }
                allocator_free(allocator, matrix);
                {
                    char msg[] = "allocator_alloc failed\n";
                    write(STDOUT_FILENO, msg, sizeof(msg));
                    munmap(memory, memory_size);
                    if (library)
                        dlclose(library);
                    return EXIT_FAILURE;
                }
            }
        }

        {
            char msg[128];
            sprintf(msg, "Factor (standard task: create dynamic matrix): %.3lf%%\n", (double)_MEM_USED / get_used_memory() * 100.);
            write(STDOUT_FILENO, msg, strlen(msg));
        }
        // Заполнение матрицы
        for (int i = 0; i < rows; i++)
        {
            for (int j = 0; j < cols; j++)
            {
                matrix[i][j] = num++;
            }
        }

        // Вывод матрицы
        for (int i = 0; i < rows; i++)
        {
            for (int j = 0; j < cols; j++)
            {
                char msg[10];
                sprintf(msg, "%4d ", matrix[i][j]);
                write(STDOUT_FILENO, msg, strlen(msg));
            }
            write(STDOUT_FILENO, "\n", 1);
        }

        // Освобождение памяти
        for (int i = 0; i < rows; i++)
        {
            allocator_free(allocator, matrix[i]);
            _MEM_USED -= cols * sizeof(int);
            matrix[i] = NULL;
        }
        allocator_free(allocator, matrix);
        _MEM_USED -= rows * sizeof(int *);
        matrix = NULL;
    }

    // Проверка фактора при сегментации
    {
        void *arr[100000];
        for (int i = 0; i < 100000; i++)
        {
            arr[i] = allocator_alloc(allocator, 173);

            _MEM_USED += 173;

            if (arr[i] == NULL)
            {
                for (int j = 0; j < i; j++)
                {
                    allocator_free(allocator, arr[j]);
                }
                {
                    char msg[] = "allocator_alloc failed\n";
                    write(STDOUT_FILENO, msg, sizeof(msg));
                    munmap(memory, memory_size);
                    if (library)
                        dlclose(library);
                    return EXIT_FAILURE;
                }
            }
        }
        {
            char msg[64];
            sprintf(msg, "Factor (many allocs): %.3lf%%\n", (double)_MEM_USED / get_used_memory() * 100.);
            write(STDOUT_FILENO, msg, strlen(msg));
        }
        for (int i = 0; i < 100000; i += 2)
        {
            allocator_free(allocator, arr[i]);
            _MEM_USED -= 173;
        }
        {
            char msg[64];
            sprintf(msg, "Factor (1/2 allocs free): %.3lf%%\n", (double)_MEM_USED / get_used_memory() * 100.);
            write(STDOUT_FILENO, msg, strlen(msg));
        }
        for (int i = 1; i < 100000; i += 2)
        {
            allocator_free(allocator, arr[i]);
            _MEM_USED -= 173;
        }
    }

    test_edge_cases(allocator, memory_size);
    test_fragmentation_list(allocator, memory_size);
    test_reuse_blocks(allocator, memory_size);
    test_alloc_and_free_time(allocator);

    allocator_destroy(allocator);

    if (munmap(memory, memory_size) == -1)
    {
        char msg[] = "munmap failed\n";
        write(STDOUT_FILENO, msg, sizeof(msg));
        if (library)
            dlclose(library);
        return EXIT_FAILURE;
    }
    if (library)
    {
        dlclose(library);
        library = NULL;
    }

    return 0;
}