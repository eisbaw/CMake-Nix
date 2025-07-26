#include <string>

extern "C" {
    const char* get_shared_message() {
        return "Hello from shared library";
    }
}