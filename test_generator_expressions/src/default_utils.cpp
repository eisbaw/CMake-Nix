// Default utilities - for configurations without specific utils

// C linkage for sprintf
extern "C" int sprintf(char*, const char*, ...);

static char utils_buffer[256];

const char* get_utils_info() {
    sprintf(utils_buffer, "Using DEFAULT utilities");
    return utils_buffer;
}