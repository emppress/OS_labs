#include "allocator.h"

#ifdef _MSC_VER
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

#define __DEBUG 1

#if __DEBUG
static size_t __USED_MEMORY = 0;
#endif

// Структура свободного блока памяти
typedef struct block_header
{
    size_t size;               // Размер блока в байтах
    struct block_header *next; // Указатель на следующий свободный блок
} block_header;

// Структура аллокатора
struct Allocator
{
    block_header *free_list_head;
    void *memory;
    size_t size;
};

// Функция для инициализации аллокатора
EXPORT Allocator *allocator_create(void *const memory, const size_t size)
{
    if (memory == NULL || size <= sizeof(Allocator) + sizeof(block_header))
    {
        return NULL;
    }
    Allocator *allocator = (Allocator *)memory;
    allocator->memory = (uint8_t *)memory + sizeof(Allocator);
    allocator->size = size - sizeof(Allocator);

    allocator->free_list_head = (block_header *)allocator->memory;
    allocator->free_list_head->size = allocator->size - sizeof(block_header);
    allocator->free_list_head->next = NULL;

#if __DEBUG
    __USED_MEMORY += sizeof(allocator) + sizeof(block_header);
#endif

    return allocator;
}

// Функция для деинициализации аллокатора
EXPORT void allocator_destroy(Allocator *const allocator)
{
    if (allocator == NULL)
    {
        return;
    }
    allocator->free_list_head = NULL;
    allocator->memory = NULL;
    allocator->size = 0;

#if __DEBUG
    __USED_MEMORY -= sizeof(allocator) + sizeof(block_header);
#endif
}

// Функция для поиска свободного блока (Best-Fit)
static block_header *find_free_block(Allocator *const allocator, size_t size)
{
    if (allocator == NULL || allocator->free_list_head == NULL)
        return NULL;

    block_header *current = allocator->free_list_head;
    block_header *best_block = NULL;
    size_t minSize = __SIZE_MAX__;

    while (current != NULL)
    {
        if (current->size >= size && current->size < minSize)
        {
            best_block = current;
            minSize = current->size;
        }
        current = current->next;
    }
    return best_block;
}

// Функция для выделения памяти
EXPORT void *allocator_alloc(Allocator *const allocator, const size_t size)
{
    if (allocator == NULL || size <= 0)
    {
        return NULL;
    }

    block_header *free_block = find_free_block(allocator, size);
    if (free_block == NULL)
    {
        return NULL; // Нет свободного блока подходящего размера
    }
    void *allocated_address = (uint8_t *)free_block + sizeof(block_header);
    // Если блок слишком большой, разделим его
    if (free_block->size > size)
    {
        // Используем адрес начала блока для нового свободного блока
        block_header *new_free_block = (block_header *)((uint8_t *)free_block + sizeof(block_header) + size);
        new_free_block->size = free_block->size - size - sizeof(block_header);
        new_free_block->next = free_block->next;
        free_block->next = new_free_block;
        free_block->size = size;

        if (free_block == allocator->free_list_head)
        {
            allocator->free_list_head = free_block->next;
        }
        else
        {
            block_header *current = allocator->free_list_head;
            while (current != NULL && current->next != free_block)
            {
                current = current->next;
            }
            if (current != NULL)
            {
                current->next = free_block->next;
            }
        }

#if __DEBUG
        __USED_MEMORY += sizeof(block_header);
#endif
    }
    else
    {
        // Удаляем текущий блок из списка свободных
        if (free_block == allocator->free_list_head)
        {
            allocator->free_list_head = free_block->next;
        }
        else
        {
            block_header *current = allocator->free_list_head;
            while (current != NULL && current->next != free_block)
            {
                current = current->next;
            }
            if (current != NULL)
            {
                current->next = free_block->next;
            }
        }
    }

#if __DEBUG
    __USED_MEMORY += size;
#endif

    return allocated_address;
}

// Слияние соседних свободных блоков
static void merge_blocks(Allocator *allocator)
{
    block_header *prev, *current;
    if (allocator == NULL)
        return; // Некорректный аллокатор или пустой список

    prev = allocator->free_list_head;
    if (prev == NULL)
        return;

    current = prev->next;
    if (current == NULL)
        return;

    while (current != NULL)
    {
        if ((uint8_t *)prev + sizeof(block_header) + prev->size == (uint8_t *)current)
        {
            prev->size += current->size + sizeof(block_header);
            prev->next = current->next;
            current = prev->next;

#if __DEBUG
            __USED_MEMORY -= sizeof(block_header);
#endif
        }
        else
        {
            prev = current;
            current = current->next;
        }
    }
}

// Функция для освобождения памяти
EXPORT void allocator_free(Allocator *const allocator, void *const memory)
{
    if (allocator == NULL || memory == NULL)
        return;

    block_header *new_free_block = (block_header *)((char *)memory - sizeof(block_header));

    // Ищем куда вставить освободившийся блок в список
    if (allocator->free_list_head == NULL || (uint8_t *)new_free_block < (uint8_t *)allocator->free_list_head)
    {
        new_free_block->next = allocator->free_list_head;
        allocator->free_list_head = new_free_block;
    }
    else
    {
        block_header *current = allocator->free_list_head;
        block_header *prev = NULL;
        while (current != NULL && (uint8_t *)new_free_block > (uint8_t *)current)
        {
            prev = current;
            current = current->next;
        }
        new_free_block->next = current;
        if (prev != NULL)
        {
            prev->next = new_free_block;
        }
    }

#if __DEBUG
    __USED_MEMORY -= new_free_block->size;
#endif

    merge_blocks(allocator);
    block_header *tmp = allocator->free_list_head;
}

#if __DEBUG
EXPORT size_t get_used_memory()
{
    return __USED_MEMORY;
}
#endif