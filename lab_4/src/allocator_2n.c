#include "allocator.h"

#ifdef _MSC_VER
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

static size_t __USED_MEMORY = 0;

// Структура для представления блока
typedef struct block_header
{
    size_t size;               // Размер блока (включая этот заголовок)
    struct block_header *next; // Указатель на следующий свободный блок в списке
} block_header;

// Структура для управления аллокатором
typedef struct Allocator
{
    void *mem_start;           // Начало области памяти mmap
    size_t mem_size;           // Размер области mmap
    block_header **free_lists; // Массив списков свободных блоков
    size_t num_lists;          // Количество списков
    size_t min_block_size;     // Минимальный размер блока (например, 8 байт)
} Allocator;

// Функция для вычисления индекса списка для заданного размера
static size_t get_list_index(Allocator const *allocator, size_t size)
{
    size_t block_size = allocator->min_block_size;
    size_t index = 0;
    while (block_size < size)
    {
        block_size <<= 1;
        index++;
    }
    return index;
}

static size_t align_to_power_of_two(size_t size)
{
    size_t current_size = 2;
    while (current_size < size)
    {
        current_size <<= 1;
    }
    return current_size;
}

// Функция для инициализации аллокатора
EXPORT Allocator *allocator_create(void *const memory, const size_t size)
{
    if (size < 512 || memory == NULL)
        return NULL;

    size_t min_block_size = align_to_power_of_two(sizeof(block_header) + 4);
    // Вычисляем количество списков
    size_t num_lists = 0;
    size_t block_size_temp = min_block_size;
    while (block_size_temp <= size)
    {
        num_lists++;
        block_size_temp <<= 1; // Умножаем на 2
    }

    // Выделяем память под структуру аллокатора и под списки
    Allocator *allocator = (Allocator *)memory;
    size_t lists_size = num_lists * sizeof(block_header *);
    allocator->free_lists = (block_header **)((uint8_t *)memory + sizeof(Allocator)); // списки после аллокатора

    allocator->mem_start = memory;
    allocator->mem_size = size;
    allocator->num_lists = num_lists;
    allocator->min_block_size = min_block_size;

    // Инициализируем списки свободных блоков
    for (size_t i = 0; i < num_lists; i++)
    {
        allocator->free_lists[i] = NULL;
    }

    // Заполняем массивы free_lists блоками, начиная с наибольшего
    size_t usable_memory_start = sizeof(Allocator) + lists_size;
    size_t current_block_start = usable_memory_start;
    size_t mem_remains = size - usable_memory_start; // Инициализируем блок размером оставшейся памяти

    for (int i = num_lists - 1; i >= 0; i--)
    {
        if (mem_remains < min_block_size)
            break;

        size_t target_block_size = min_block_size << i;
        if (current_block_start < size && target_block_size <= mem_remains)
        {

            block_header *block = (block_header *)((uint8_t *)memory + current_block_start);
            __USED_MEMORY += sizeof(block_header);
            block->size = target_block_size;
            block->next = NULL;

            allocator->free_lists[i] = block;         // Помещаем блок в нужный список
            current_block_start += target_block_size; // Обновляем позицию следующего блока
            mem_remains -= target_block_size;         // Уменьшаем размер доступной памяти
        }
    }
    __USED_MEMORY += usable_memory_start;

    return allocator;
}

// Функция для выделения памяти
EXPORT void *allocator_alloc(Allocator *const allocator, const size_t _size)
{
    if (allocator == NULL)
        return NULL;

    // Учитываем заголовок блока при выделении
    size_t size = align_to_power_of_two(_size + sizeof(block_header));
    size_t index = get_list_index(allocator, size);

    if (index >= allocator->num_lists)
        return NULL;

    block_header *block = NULL;
    if (allocator->free_lists[index])
    {
        block = allocator->free_lists[index];
        allocator->free_lists[index] = allocator->free_lists[index]->next;
        __USED_MEMORY += size - sizeof(block_header);
        return (void *)((uint8_t *)block + sizeof(block_header));
    }

    // Если не нашли блок нужного размера, ищем в списках с большими блоками
    for (size_t i = index + 1; i < allocator->num_lists; i++)
    {
        if (allocator->free_lists[i] != NULL)
        {
            block = allocator->free_lists[i];
            allocator->free_lists[i] = block->next;
            // Разбиваем блок
            while (block->size > size)
            {
                size_t split_size = block->size >> 1; // Делим блок пополам

                if (split_size < size)
                {
                    break; // Нельзя разбить блок на блок нужного размера
                }

                block->size = split_size;

                block_header *new_block = (block_header *)((uint8_t *)block + split_size);

                __USED_MEMORY += sizeof(block_header);

                new_block->size = split_size;
                // Помещаем новый блок в соответствующий список
                size_t new_block_index = get_list_index(allocator, split_size);
                new_block->next = allocator->free_lists[new_block_index];
                allocator->free_lists[new_block_index] = new_block;
            }
            break;
        }
    }

    if (block == NULL)
        return NULL;

    __USED_MEMORY += size - sizeof(block_header);
    return (void *)((uint8_t *)block + sizeof(block_header));
}

EXPORT void allocator_free(Allocator *const allocator, void *const memory)
{
    if (allocator == NULL || memory == NULL)
        return;

    block_header *block = (block_header *)((uint8_t *)memory - sizeof(block_header));

    size_t index = get_list_index(allocator, block->size);
    if (index >= allocator->num_lists)
        return;

    block->next = allocator->free_lists[index];
    allocator->free_lists[index] = block;

    __USED_MEMORY -= block->size - sizeof(block_header);
}

EXPORT void allocator_destroy(Allocator *const allocator)
{
    if (allocator == NULL)
        return;

    allocator->free_lists = NULL;
    allocator->mem_size = 0;
    allocator->mem_start = NULL;
    allocator->min_block_size = 0;
    allocator->num_lists = 0;
}

EXPORT size_t get_used_memory()
{
    return __USED_MEMORY;
}