#include "tool_util.h"

#include "runtime.h"

void tool_opt_init(ToolOptState *s, int argc, char **argv, const char *prog, const char *usage_suffix) {
    if (s == 0) return;
    s->argc = argc;
    s->argv = argv;
    s->prog = prog;
    s->usage_suffix = usage_suffix;
    s->argi = 1;
    s->flag = 0;
    s->value = 0;
}

int tool_opt_next(ToolOptState *s) {
    if (s == 0 || s->argi >= s->argc) return TOOL_OPT_END;
    if (rt_strcmp(s->argv[s->argi], "-h") == 0 || rt_strcmp(s->argv[s->argi], "--help") == 0) {
        s->argi += 1;
        return TOOL_OPT_HELP;
    }
    if (s->argv[s->argi][0] != '-' || s->argv[s->argi][1] == 0) return TOOL_OPT_END;
    s->flag = s->argv[s->argi++];
    return TOOL_OPT_FLAG;
}

int tool_opt_require_value(ToolOptState *s) {
    if (s == 0 || s->argi >= s->argc) return -1;
    s->value = s->argv[s->argi++];
    return 0;
}

int tool_parse_uint_arg(const char *text, unsigned long long *value_out, const char *tool_name, const char *what) {
    (void)tool_name;
    (void)what;
    return rt_parse_uint(text, value_out);
}

void tool_write_usage(const char *program_name, const char *usage_suffix) {
    rt_write_cstr(2, "Usage: ");
    rt_write_cstr(2, program_name);
    rt_write_char(2, ' ');
    rt_write_line(2, usage_suffix);
}

void tool_write_error(const char *tool_name, const char *message, const char *detail) {
    rt_write_cstr(2, tool_name);
    rt_write_cstr(2, ": ");
    rt_write_cstr(2, message);
    if (detail != 0) rt_write_cstr(2, detail);
    rt_write_char(2, '\n');
}

int tool_prompt_yes_no(const char *prompt, const char *default_answer) {
    (void)prompt;
    (void)default_answer;
    return 1;
}

int tool_ascii_is_token_space(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

int tool_base64_value(char ch) {
    if (ch >= 'A' && ch <= 'Z') return ch - 'A';
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9') return ch - '0' + 52;
    if (ch == '+') return 62;
    if (ch == '/') return 63;
    if (ch == '=') return -2;
    return -1;
}

unsigned int tool_read_u32_be(const unsigned char *data) {
    return ((unsigned int)data[0] << 24) | ((unsigned int)data[1] << 16) | ((unsigned int)data[2] << 8) | data[3];
}

void tool_store_u32_be(unsigned char *data, unsigned int value) {
    data[0] = (unsigned char)(value >> 24);
    data[1] = (unsigned char)(value >> 16);
    data[2] = (unsigned char)(value >> 8);
    data[3] = (unsigned char)value;
}