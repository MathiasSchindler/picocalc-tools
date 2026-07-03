#include "linux_sys.h"

#define INPUT_BUFFER_SIZE 128U

static unsigned char g_input_buffer[INPUT_BUFFER_SIZE];
static size_t g_input_offset;
static size_t g_input_count;

void solve_repl_input_init(void) {
    g_input_offset = 0U;
    g_input_count = 0U;
}

int solve_repl_read_key(void) {
    while (g_input_offset >= g_input_count) {
        long count = sim_linux_read(0, g_input_buffer, sizeof(g_input_buffer));
        if (count <= 0) return -1;
        g_input_offset = 0U;
        g_input_count = (size_t)count;
    }
    return g_input_buffer[g_input_offset++];
}

void solve_repl_mirror_write(const char *data, size_t count) {
    (void)sim_linux_write(1, data, count);
}