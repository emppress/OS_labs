#include "allocator.h"

#include <unistd.h>
#include <dlfcn.h>

static allocator_alloc_f *allocator_alloc;
static allocator_create_f *allocator_create;
static allocator_destroy_f *allocator_destroy;
static allocator_free_f *allocator_free;

typedef struct fallback_allocator
{
    size_t size;
    void *memory;
} fallback_allocator;

Allocator *fallback_allocator_create(void *const memory, const size_t size)
{
    if (memory == NULL || size <= sizeof(fallback_allocator))
    {
        return NULL;
    }
    fallback_allocator *alloc = (fallback_allocator *)memory;
    alloc->memory = memory;
    alloc->size = size;
    return (Allocator *)alloc;
}

void fallback_allocator_destroy(Allocator *const allocator)
{
    if (allocator == NULL)
    {
        return;
    }

    munmap(((fallback_allocator *)allocator)->memory, ((fallback_allocator *)allocator)->size);
}

void *fallback_allocator_alloc(Allocator *const allocator, const size_t size)
{
    if (allocator == NULL || size == 0)
    {
        return NULL;
    }

    static size_t offset = sizeof(fallback_allocator);

    if (offset + size > ((fallback_allocator *)allocator)->size)
    {
        return NULL;
    }

    void *allocated_memory = (void *)((char *)((fallback_allocator *)allocator)->memory + offset);
    offset += size;

    return allocated_memory;
}

void fallback_allocator_free(Allocator *const allocator, void *const memory)
{
    (void)allocator;
    (void)memory;
}

int main(int argc, char *argv[])
{
    void *library;
    if (argc == 1)
    {
        allocator_create = fallback_allocator_create;
        allocator_destroy = fallback_allocator_destroy;
        allocator_alloc = fallback_allocator_alloc;
        allocator_free = fallback_allocator_free;
    }
    else
    {
        library = dlopen(argv[1], RTLD_LOCAL | RTLD_NOW);
        if (library)
        {
            allocator_create = dlsym(library, "allocator_create");
            allocator_destroy = dlsym(library, "allocator_destroy");
            allocator_alloc = dlsym(library, "allocator_alloc");
            allocator_free = dlsym(library, "allocator_free");
        }
        if (!library || !allocator_create || !allocator_destroy || !allocator_alloc || !allocator_free)
        {
            allocator_create = fallback_allocator_create;
            allocator_destroy = fallback_allocator_destroy;
            allocator_alloc = fallback_allocator_alloc;
            allocator_free = fallback_allocator_free;
        }
    }

    size_t memorySize = 10000000000;
    void *memory = mmap(NULL, memorySize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED)
    {
        perror("mmap failed");
        return EXIT_FAILURE;
    }

    Allocator *allocator = allocator_create(memory, memorySize);
    if (allocator == NULL)
    {
        return EXIT_FAILURE;
    }

    int *ptr1 = (int *)allocator_alloc(allocator, sizeof(int));
    if (ptr1 != NULL)
    {
        *ptr1 = 1000;
    }

    double *ptr2 = (double *)allocator_alloc(allocator, sizeof(double));
    if (ptr2 != NULL)
    {
        *ptr2 = 1021.12;
    }

    char *ptr3 = (char *)allocator_alloc(allocator, 8589934555);
    if (ptr3 != NULL)
    {
        sprintf(ptr3, "bibibibibibibibibibibibibibibibibibibibbibibibi");
    }

    void *ptr4 = allocator_alloc(allocator, 1024);
    if (ptr4 != NULL)
    {
        printf("Выделено 1024 байт по адресу: %p\n", ptr4);
    }

    void *ptr5 = allocator_alloc(allocator, 100);
    if (ptr5 != NULL)
    {
        printf("Выделено 100 байт по адресу: %p\n", ptr5);
    }

    void *ptr6 = allocator_alloc(allocator, 100);
    if (ptr6 != NULL)
    {
        printf("Выделено 100 байт по адресу: %p\n", ptr6);
    }

    printf("\n\n **%d %lf %s**\n\n", *ptr1, *ptr2, ptr3);
    allocator_free(allocator, ptr1);

    allocator_free(allocator, ptr2);

    allocator_free(allocator, ptr3);

    allocator_free(allocator, ptr5);

    allocator_free(allocator, ptr6);

    allocator_free(allocator, ptr4);

    allocator_destroy(allocator);
    return 0;
}