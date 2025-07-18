#include "core/entity.h"
#include <stdio.h>
#include <string.h>

error_code_t entity_manager_init(entity_manager_t* mgr, size_t initial_capacity) {
    if (!mgr || initial_capacity == 0) {
        return ERROR_INVALID_INPUT;
    }
    
    mgr->pool = memory_pool_create(initial_capacity * sizeof(entity_t), 8);
    if (!mgr->pool) {
        return ERROR_OUT_OF_MEMORY;
    }
    
    mgr->entities = (entity_t*)memory_pool_alloc(mgr->pool, 
                                                initial_capacity * sizeof(entity_t));
    if (!mgr->entities) {
        memory_pool_destroy(mgr->pool);
        return ERROR_OUT_OF_MEMORY;
    }
    
    mgr->capacity = initial_capacity;
    mgr->count = 0;
    
    memset(mgr->entities, 0, initial_capacity * sizeof(entity_t));
    
    printf("Entity manager initialized with capacity %zu\n", initial_capacity);
    return SUCCESS;
}

void entity_manager_destroy(entity_manager_t* mgr) {
    if (mgr && mgr->pool) {
        printf("Destroying entity manager (%zu entities)\n", mgr->count);
        memory_pool_destroy(mgr->pool);
        mgr->entities = NULL;
        mgr->capacity = 0;
        mgr->count = 0;
    }
}

entity_id_t entity_create(entity_manager_t* mgr) {
    if (!mgr || mgr->count >= mgr->capacity) {
        return 0; // Invalid ID
    }
    
    entity_t* entity = &mgr->entities[mgr->count];
    entity->id = mgr->count + 1; // 1-based IDs
    entity->component_mask = 0;
    memset(entity->components, 0, sizeof(entity->components));
    
    mgr->count++;
    printf("Created entity with ID %u\n", entity->id);
    return entity->id;
}

error_code_t entity_destroy(entity_manager_t* mgr, entity_id_t id) {
    if (!mgr || id == 0 || id > mgr->count) {
        return ERROR_INVALID_INPUT;
    }
    
    // Simple implementation: mark as invalid (real implementation would compact)
    entity_t* entity = &mgr->entities[id - 1];
    entity->id = 0;
    entity->component_mask = 0;
    
    printf("Destroyed entity with ID %u\n", id);
    return SUCCESS;
}