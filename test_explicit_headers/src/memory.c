#include "platform/memory.h"
#include <stdio.h>
#include <string.h>

static size_t total_allocated = 0;

memory_pool_t* memory_pool_create(size_t size, uint32_t alignment) {
    if (size == 0 || alignment == 0) {
        return NULL;
    }
    
    memory_pool_t* pool = malloc(sizeof(memory_pool_t));
    if (!pool) {
        return NULL;
    }
    
    // Align size to alignment boundary
    size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
    
    pool->memory = malloc(aligned_size);
    if (!pool->memory) {
        free(pool);
        return NULL;
    }
    
    pool->size = aligned_size;
    pool->used = 0;
    pool->alignment = alignment;
    
    total_allocated += aligned_size;
    
    printf("Memory pool created: %zu bytes (aligned to %u)\n", aligned_size, alignment);
    return pool;
}

void memory_pool_destroy(memory_pool_t* pool) {
    if (pool) {
        printf("Memory pool destroyed: %zu bytes used of %zu\n", pool->used, pool->size);
        total_allocated -= pool->size;
        free(pool->memory);
        free(pool);
    }
}

void* memory_pool_alloc(memory_pool_t* pool, size_t size) {
    if (!pool || size == 0) {
        return NULL;
    }
    
    // Align allocation size
    size_t aligned_size = (size + pool->alignment - 1) & ~(pool->alignment - 1);
    
    if (pool->used + aligned_size > pool->size) {
        printf("Memory pool allocation failed: not enough space\n");
        return NULL;
    }
    
    void* ptr = (char*)pool->memory + pool->used;
    pool->used += aligned_size;
    
    return ptr;
}

void memory_pool_reset(memory_pool_t* pool) {
    if (pool) {
        pool->used = 0;
        printf("Memory pool reset\n");
    }
}

#ifdef DEBUG_MEMORY
void memory_dump_stats(void) {
    printf("Total allocated memory: %zu bytes\n", total_allocated);
}

size_t memory_get_total_allocated(void) {
    return total_allocated;
}
#endif