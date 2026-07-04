#ifndef PICOCALC_SSH_TOOL_UTIL_H
#define PICOCALC_SSH_TOOL_UTIL_H

#include <stddef.h>

typedef struct {
    int argc;
    char **argv;
    const char *prog;
    const char *usage_suffix;
    int argi;
    const char *flag;
    const char *value;
} ToolOptState;

#define TOOL_OPT_END   0
#define TOOL_OPT_FLAG  1
#define TOOL_OPT_HELP  2
#define TOOL_OPT_ERROR 3

void tool_opt_init(ToolOptState *s, int argc, char **argv, const char *prog, const char *usage_suffix);
int tool_opt_next(ToolOptState *s);
int tool_opt_require_value(ToolOptState *s);
int tool_parse_uint_arg(const char *text, unsigned long long *value_out, const char *tool_name, const char *what);
void tool_write_usage(const char *program_name, const char *usage_suffix);
void tool_write_error(const char *tool_name, const char *message, const char *detail);
int tool_prompt_yes_no(const char *prompt, const char *default_answer);
int tool_ascii_is_token_space(char ch);
int tool_base64_value(char ch);
unsigned int tool_read_u32_be(const unsigned char *data);
void tool_store_u32_be(unsigned char *data, unsigned int value);

#endif