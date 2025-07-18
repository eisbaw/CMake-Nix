#include <stdio.h>
#include <math.h>
#include <pthread.h>

void* worker_thread(void* arg) {
    double* result = (double*)arg;
    *result = sqrt(42.0);
    return NULL;
}

int main() {
    printf("Testing external libraries with pure Nix approach\n");
    
    // Test math library
    double value = sin(3.14159 / 2);
    printf("sin(π/2) = %f\n", value);
    
    // Test pthread library
    pthread_t thread;
    double result = 0.0;
    pthread_create(&thread, NULL, worker_thread, &result);
    pthread_join(thread, NULL);
    printf("sqrt(42) = %f\n", result);
    
    printf("External library test completed successfully!\n");
    return 0;
}
