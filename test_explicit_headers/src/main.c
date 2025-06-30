#include "core/entity.h"
#include "platform/memory.h"
#include <stdio.h>

int main() {
    printf("Entity Component System Test\n");
    printf("============================\n");
    
    // Initialize entity manager
    entity_manager_t mgr;
    error_code_t result = entity_manager_init(&mgr, 10);
    if (result != SUCCESS) {
        printf("Failed to initialize entity manager: %d\n", result);
        return 1;
    }
    
    // Create some entities
    entity_id_t entity1 = entity_create(&mgr);
    entity_id_t entity2 = entity_create(&mgr);
    entity_id_t entity3 = entity_create(&mgr);
    
    printf("\nCreated entities: %u, %u, %u\n", entity1, entity2, entity3);
    
    // Test memory pool functionality
    memory_pool_t* test_pool = memory_pool_create(1024, 16);
    if (test_pool) {
        void* ptr1 = memory_pool_alloc(test_pool, 64);
        void* ptr2 = memory_pool_alloc(test_pool, 128);
        printf("Allocated memory at %p and %p\n", ptr1, ptr2);
        memory_pool_destroy(test_pool);
    }
    
    // Destroy an entity
    entity_destroy(&mgr, entity2);
    
#ifdef DEBUG_MEMORY
    memory_dump_stats();
#endif
    
    // Cleanup
    entity_manager_destroy(&mgr);
    
    printf("\nTest completed successfully!\n");
    return 0;
}