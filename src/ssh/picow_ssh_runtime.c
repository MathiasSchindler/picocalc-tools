#include "runtime.h"

#include "platform.h"

#include <stdint.h>
#include <string.h>

#define SSH_HEAP_SIZE (64u * 1024u)

static unsigned char g_heap[SSH_HEAP_SIZE];
static size_t g_heap_used;

static size_t align_size(size_t value) {
    return (value + 7u) & ~(size_t)7u;
}

void *rt_malloc(size_t size) {
    size_t aligned = align_size(size == 0u ? 1u : size);
    void *ptr;
    if (aligned > sizeof(g_heap) || g_heap_used > sizeof(g_heap) - aligned) return 0;
    ptr = g_heap + g_heap_used;
    g_heap_used += aligned;
    return ptr;
}

void *rt_realloc(void *ptr, size_t size) {
    void *new_ptr;
    if (ptr == 0) return rt_malloc(size);
    new_ptr = rt_malloc(size);
    return new_ptr;
}

void rt_free(void *ptr) {
    (void)ptr;
}

void *rt_malloc_array(size_t count, size_t item_size) {
    if (item_size != 0u && count > ((size_t)-1) / item_size) return 0;
    return rt_malloc(count * item_size);
}

void *rt_realloc_array(void *ptr, size_t count, size_t item_size) {
    if (item_size != 0u && count > ((size_t)-1) / item_size) return 0;
    return rt_realloc(ptr, count * item_size);
}

size_t rt_strlen(const char *text) {
    size_t len = 0;
    if (text == 0) return 0;
    while (text[len] != 0) len += 1;
    return len;
}

int rt_strcmp(const char *lhs, const char *rhs) {
    unsigned char a;
    unsigned char b;
    if (lhs == 0) lhs = "";
    if (rhs == 0) rhs = "";
    do {
        a = (unsigned char)*lhs++;
        b = (unsigned char)*rhs++;
        if (a != b) return a < b ? -1 : 1;
    } while (a != 0);
    return 0;
}

int rt_strncmp(const char *lhs, const char *rhs, size_t count) {
    size_t i;
    if (lhs == 0) lhs = "";
    if (rhs == 0) rhs = "";
    for (i = 0; i < count; ++i) {
        unsigned char a = (unsigned char)lhs[i];
        unsigned char b = (unsigned char)rhs[i];
        if (a != b) return a < b ? -1 : 1;
        if (a == 0) return 0;
    }
    return 0;
}

void rt_copy_string(char *dst, size_t dst_size, const char *src) {
    size_t i = 0;
    if (dst == 0 || dst_size == 0) return;
    if (src == 0) src = "";
    while (i + 1u < dst_size && src[i] != 0) {
        dst[i] = src[i];
        i += 1;
    }
    dst[i] = 0;
}

int rt_join_path(const char *dir_path, const char *name, char *buffer, size_t buffer_size) {
    size_t dir_len = rt_strlen(dir_path);
    size_t name_len = rt_strlen(name);
    int need_slash = dir_len != 0u && dir_path[dir_len - 1u] != '/';
    if (buffer == 0 || dir_path == 0 || name == 0 || dir_len + (size_t)need_slash + name_len + 1u > buffer_size) return -1;
    memcpy(buffer, dir_path, dir_len);
    if (need_slash) buffer[dir_len++] = '/';
    memcpy(buffer + dir_len, name, name_len);
    buffer[dir_len + name_len] = 0;
    return 0;
}

void rt_unsigned_to_string(unsigned long long value, char *buffer, size_t buffer_size) {
    char tmp[32];
    size_t used = 0;
    if (buffer == 0 || buffer_size == 0) return;
    do {
        tmp[used++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && used < sizeof(tmp));
    if (used + 1u > buffer_size) used = buffer_size - 1u;
    for (size_t i = 0; i < used; ++i) buffer[i] = tmp[used - 1u - i];
    buffer[used] = 0;
}

void rt_memset(void *buffer, int byte_value, size_t count) {
    memset(buffer, byte_value, count);
}

int rt_parse_uint(const char *text, unsigned long long *value_out) {
    unsigned long long value = 0;
    size_t i = 0;
    if (text == 0 || text[0] == 0 || value_out == 0) return -1;
    while (text[i] != 0) {
        unsigned int digit;
        if (text[i] < '0' || text[i] > '9') return -1;
        digit = (unsigned int)(text[i] - '0');
        if (value > (((unsigned long long)-1) - digit) / 10u) return -1;
        value = value * 10u + digit;
        i += 1;
    }
    *value_out = value;
    return 0;
}

int rt_write_all(int fd, const void *data, size_t count) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t done = 0;
    while (done < count) {
        long r = platform_write(fd, bytes + done, count - done);
        if (r <= 0) return -1;
        done += (size_t)r;
    }
    return 0;
}

int rt_write_cstr(int fd, const char *text) {
    return rt_write_all(fd, text, rt_strlen(text));
}

int rt_write_line(int fd, const char *text) {
    return rt_write_cstr(fd, text) == 0 && rt_write_char(fd, '\n') == 0 ? 0 : -1;
}

int rt_write_char(int fd, char ch) {
    return rt_write_all(fd, &ch, 1u);
}

int rt_write_uint(int fd, unsigned long long value) {
    char text[32];
    rt_unsigned_to_string(value, text, sizeof(text));
    return rt_write_cstr(fd, text);
}

int rt_write_int(int fd, long long value) {
    if (value < 0) {
        unsigned long long magnitude = (unsigned long long)(-(value + 1)) + 1u;
        return rt_write_char(fd, '-') == 0 && rt_write_uint(fd, magnitude) == 0 ? 0 : -1;
    }
    return rt_write_uint(fd, (unsigned long long)value);
}