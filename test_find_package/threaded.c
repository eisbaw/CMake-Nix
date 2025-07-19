#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

void* thread_function(void* arg) {
    int id = *(int*)arg;
    printf("Thread %d running\n", id);
    sleep(1);
    printf("Thread %d finishing\n", id);
    return NULL;
}

int main() {
    printf("Testing pthread support\n");
    
    pthread_t threads[2];
    int ids[2] = {1, 2};
    
    for (int i = 0; i < 2; i++) {
        if (pthread_create(&threads[i], NULL, thread_function, &ids[i]) != 0) {
            printf("Error creating thread %d\n", i);
            return 1;
        }
    }
    
    for (int i = 0; i < 2; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("All threads completed\n");
    return 0;
}