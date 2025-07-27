#include "mylib.h"

// C linkage for sprintf
extern "C" int sprintf(char*, const char*, ...);

// Simple strlen implementation to avoid header dependency
static unsigned long my_strlen(const char* str) {
    unsigned long len = 0;
    while (str[len]) len++;
    return len;
}

// Simple strcat implementation
static void my_strcat(char* dest, const char* src) {
    unsigned long dest_len = my_strlen(dest);
    unsigned long i = 0;
    while (src[i]) {
        dest[dest_len + i] = src[i];
        i++;
    }
    dest[dest_len + i] = '\0';
}

// Static buffer for build info
static char build_info_buffer[256];

const char* get_build_info() {
    sprintf(build_info_buffer, "Built with: ");
    
#ifdef DEBUG_MODE
    my_strcat(build_info_buffer, "DEBUG_MODE ");
#endif
#ifdef RELEASE_MODE
    my_strcat(build_info_buffer, "RELEASE_MODE ");
#endif
#ifdef RELWITHDEBINFO_MODE
    my_strcat(build_info_buffer, "RELWITHDEBINFO_MODE ");
#endif
#ifdef MINSIZEREL_MODE
    my_strcat(build_info_buffer, "MINSIZEREL_MODE ");
#endif
    
    return build_info_buffer;
}