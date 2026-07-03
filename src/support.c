#include "runtime.h"
#include "tool_util.h"

long solve_platform_write(int fd, const void *data, size_t count);

static int g_tool_color_mode = TOOL_COLOR_NEVER;
static int g_tool_json_enabled = 0;
static unsigned long long g_tool_json_sequence = 0ULL;

#ifdef PICOCALC_SOLVE_PROVIDE_MEMOPS
void *memcpy(void *dst, const void *src, size_t count) {
    unsigned char *out = (unsigned char *)dst;
    const unsigned char *in = (const unsigned char *)src;
    while (count > 0U) {
        *out++ = *in++;
        count -= 1U;
    }
    return dst;
}

int memcmp(const void *left, const void *right, size_t count) {
    const unsigned char *lhs = (const unsigned char *)left;
    const unsigned char *rhs = (const unsigned char *)right;
    while (count > 0U) {
        if (*lhs != *rhs) return (int)*lhs - (int)*rhs;
        lhs += 1;
        rhs += 1;
        count -= 1U;
    }
    return 0;
}

void *memmove(void *dst, const void *src, size_t count) {
    unsigned char *out = (unsigned char *)dst;
    const unsigned char *in = (const unsigned char *)src;
    if (out > in && out < in + count) {
        while (count > 0U) {
            count -= 1U;
            out[count] = in[count];
        }
    } else {
        (void)memcpy(dst, src, count);
    }
    return dst;
}

void *memset(void *buffer, int byte_value, size_t count) {
    unsigned char *out = (unsigned char *)buffer;
    while (count > 0U) {
        *out++ = (unsigned char)byte_value;
        count -= 1U;
    }
    return buffer;
}
#endif

size_t rt_strlen(const char *text) {
    size_t length = 0U;
    while (text[length] != '\0') length += 1U;
    return length;
}

int rt_strcmp(const char *lhs, const char *rhs) {
    while (*lhs != '\0' && *lhs == *rhs) {
        lhs += 1;
        rhs += 1;
    }
    return (unsigned char)*lhs - (unsigned char)*rhs;
}

int rt_strncmp(const char *lhs, const char *rhs, size_t count) {
    while (count > 0U && *lhs != '\0' && *lhs == *rhs) {
        lhs += 1;
        rhs += 1;
        count -= 1U;
    }
    if (count == 0U) return 0;
    return (unsigned char)*lhs - (unsigned char)*rhs;
}

void rt_copy_string(char *dst, size_t dst_size, const char *src) {
    size_t used = 0U;
    if (dst_size == 0U) return;
    while (used + 1U < dst_size && src[used] != '\0') {
        dst[used] = src[used];
        used += 1U;
    }
    dst[used] = '\0';
}

void rt_memset(void *buffer, int byte_value, size_t count) {
    (void)memset(buffer, byte_value, count);
}

int rt_parse_uint(const char *text, unsigned long long *value_out) {
    unsigned long long value = 0ULL;
    int saw_digit = 0;
    while (*text >= '0' && *text <= '9') {
        unsigned long long digit = (unsigned long long)(*text - '0');
        if (value > (18446744073709551615ULL - digit) / 10ULL) return -1;
        value = value * 10ULL + digit;
        saw_digit = 1;
        text += 1;
    }
    if (!saw_digit || *text != '\0') return -1;
    *value_out = value;
    return 0;
}

int rt_write_all(int fd, const void *data, size_t count) {
    const unsigned char *bytes = (const unsigned char *)data;
    while (count > 0U) {
        long written = solve_platform_write(fd, bytes, count);
        if (written <= 0) return -1;
        bytes += (size_t)written;
        count -= (size_t)written;
    }
    return 0;
}

int rt_write_cstr(int fd, const char *text) {
    return rt_write_all(fd, text, rt_strlen(text));
}

int rt_write_line(int fd, const char *text) {
    if (rt_write_cstr(fd, text) != 0) return -1;
    return rt_write_char(fd, '\n');
}

int rt_write_char(int fd, char ch) {
    return rt_write_all(fd, &ch, 1U);
}

int rt_write_uint(int fd, unsigned long long value) {
    char digits[24];
    size_t count = 0U;
    if (value == 0ULL) return rt_write_char(fd, '0');
    while (value > 0ULL && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }
    while (count > 0U) {
        if (rt_write_char(fd, digits[--count]) != 0) return -1;
    }
    return 0;
}

void tool_set_global_color_mode(int mode) {
    g_tool_color_mode = mode;
}

int tool_get_global_color_mode(void) {
    return g_tool_color_mode;
}

int tool_should_use_color_fd(int fd, int mode) {
    (void)fd;
    (void)mode;
    return 0;
}

void tool_write_styled(int fd, int mode, int style, const char *text) {
    (void)mode;
    (void)style;
    (void)rt_write_cstr(fd, text);
}

int tool_json_is_enabled(void) {
    return g_tool_json_enabled;
}

int tool_json_begin_event(int fd, const char *tool_name, const char *stream_name, const char *event_name) {
    g_tool_json_sequence += 1ULL;
    if (rt_write_cstr(fd, "{\"schema\":\"newos.tool.v1\",\"tool\":") != 0) return -1;
    if (tool_json_write_string(fd, tool_name != 0 ? tool_name : "tool") != 0) return -1;
    if (rt_write_cstr(fd, ",\"stream\":") != 0) return -1;
    if (tool_json_write_string(fd, stream_name != 0 ? stream_name : (fd == 2 ? "stderr" : "stdout")) != 0) return -1;
    if (rt_write_cstr(fd, ",\"event\":") != 0) return -1;
    if (tool_json_write_string(fd, event_name != 0 ? event_name : "event") != 0) return -1;
    if (rt_write_cstr(fd, ",\"seq\":") != 0) return -1;
    return rt_write_uint(fd, g_tool_json_sequence);
}

int tool_json_end_event(int fd) {
    return rt_write_cstr(fd, "}\n");
}

int tool_json_write_string(int fd, const char *text) {
    if (rt_write_char(fd, '"') != 0) return -1;
    while (text != 0 && *text != '\0') {
        if (*text == '"' || *text == '\\') {
            if (rt_write_char(fd, '\\') != 0) return -1;
            if (rt_write_char(fd, *text) != 0) return -1;
        } else if (*text == '\n') {
            if (rt_write_cstr(fd, "\\n") != 0) return -1;
        } else if (*text == '\r') {
            if (rt_write_cstr(fd, "\\r") != 0) return -1;
        } else if (*text == '\t') {
            if (rt_write_cstr(fd, "\\t") != 0) return -1;
        } else {
            if (rt_write_char(fd, *text) != 0) return -1;
        }
        text += 1;
    }
    return rt_write_char(fd, '"');
}

void tool_opt_init(ToolOptState *state, int argc, char **argv, const char *prog, const char *usage_suffix) {
    state->argc = argc;
    state->argv = argv;
    state->prog = prog;
    state->usage_suffix = usage_suffix;
    state->argi = 1;
    state->flag = 0;
    state->value = 0;
}

int tool_opt_next(ToolOptState *state) {
    const char *arg;
    while (1) {
        state->flag = 0;
        state->value = 0;
        if (state->argi >= state->argc) return TOOL_OPT_END;
        arg = state->argv[state->argi];
        if (arg[0] != '-' || arg[1] == '\0') return TOOL_OPT_END;
        state->argi += 1;
        if (rt_strcmp(arg, "--") == 0) return TOOL_OPT_END;
        if (rt_strcmp(arg, "--json") == 0) {
            g_tool_json_enabled = 1;
            g_tool_color_mode = TOOL_COLOR_NEVER;
            continue;
        }
        if (rt_strcmp(arg, "-h") == 0 || rt_strcmp(arg, "--help") == 0) return TOOL_OPT_HELP;
        state->flag = arg;
        return TOOL_OPT_FLAG;
    }
}

int tool_opt_require_value(ToolOptState *state) {
    const char *equal;
    if (state->flag != 0) {
        for (equal = state->flag; *equal != '\0'; ++equal) {
            if (*equal == '=') {
                state->value = equal + 1;
                return 0;
            }
        }
    }
    if (state->argi >= state->argc) {
        tool_write_error(state->prog, "missing value for ", state->flag);
        return -1;
    }
    state->value = state->argv[state->argi++];
    return 0;
}

const char *tool_base_name(const char *path) {
    const char *base = path;
    while (*path != '\0') {
        if (*path == '/') base = path + 1;
        path += 1;
    }
    return base;
}

void tool_write_usage(const char *program_name, const char *usage_suffix) {
    (void)rt_write_cstr(2, "usage: ");
    (void)rt_write_cstr(2, program_name);
    (void)rt_write_char(2, ' ');
    (void)rt_write_line(2, usage_suffix);
}

void tool_write_error(const char *tool_name, const char *message, const char *detail) {
    (void)rt_write_cstr(2, tool_name);
    (void)rt_write_cstr(2, ": ");
    (void)rt_write_cstr(2, message);
    if (detail != 0) (void)rt_write_cstr(2, detail);
    (void)rt_write_char(2, '\n');
}

int tool_starts_with(const char *text, const char *prefix) {
    while (*prefix != '\0') {
        if (*text != *prefix) return 0;
        text += 1;
        prefix += 1;
    }
    return 1;
}

int tool_ascii_is_space(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f';
}

int tool_ascii_is_identifier_start(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
}

int tool_ascii_is_identifier_char(char ch) {
    return tool_ascii_is_identifier_start(ch) || (ch >= '0' && ch <= '9');
}

int tool_buffer_append_char_checked(char *buffer, size_t buffer_size, size_t *length_io, char ch) {
    size_t length = *length_io;
    if (length + 1U >= buffer_size) return -1;
    buffer[length++] = ch;
    buffer[length] = '\0';
    *length_io = length;
    return 0;
}

int tool_buffer_append_text_checked(char *buffer, size_t buffer_size, size_t *length_io, const char *text) {
    while (*text != '\0') {
        if (tool_buffer_append_char_checked(buffer, buffer_size, length_io, *text) != 0) return -1;
        text += 1;
    }
    return 0;
}