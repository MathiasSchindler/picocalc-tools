#ifndef PICOCALC_SOLVE_TOOL_UTIL_H
#define PICOCALC_SOLVE_TOOL_UTIL_H

#include <stddef.h>

typedef enum {
    TOOL_COLOR_NEVER = 0,
    TOOL_COLOR_AUTO = 1,
    TOOL_COLOR_ALWAYS = 2
} ToolColorMode;

typedef enum {
    TOOL_STYLE_PLAIN = 0,
    TOOL_STYLE_BOLD,
    TOOL_STYLE_RED,
    TOOL_STYLE_GREEN,
    TOOL_STYLE_YELLOW,
    TOOL_STYLE_BLUE,
    TOOL_STYLE_MAGENTA,
    TOOL_STYLE_CYAN,
    TOOL_STYLE_BOLD_RED,
    TOOL_STYLE_BOLD_GREEN,
    TOOL_STYLE_BOLD_YELLOW,
    TOOL_STYLE_BOLD_BLUE,
    TOOL_STYLE_BOLD_MAGENTA,
    TOOL_STYLE_BOLD_CYAN,
    TOOL_STYLE_BOLD_WHITE
} ToolTextStyle;

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

void tool_set_global_color_mode(int mode);
int tool_get_global_color_mode(void);
int tool_should_use_color_fd(int fd, int mode);
void tool_write_styled(int fd, int mode, int style, const char *text);

int tool_json_is_enabled(void);
int tool_json_begin_event(int fd, const char *tool_name, const char *stream_name, const char *event_name);
int tool_json_end_event(int fd);
int tool_json_write_string(int fd, const char *text);

void tool_opt_init(ToolOptState *state, int argc, char **argv, const char *prog, const char *usage_suffix);
int tool_opt_next(ToolOptState *state);
int tool_opt_require_value(ToolOptState *state);

const char *tool_base_name(const char *path);
void tool_write_usage(const char *program_name, const char *usage_suffix);
void tool_write_error(const char *tool_name, const char *message, const char *detail);
int tool_starts_with(const char *text, const char *prefix);

int tool_ascii_is_space(char ch);
int tool_ascii_is_identifier_start(char ch);
int tool_ascii_is_identifier_char(char ch);

int tool_buffer_append_char_checked(char *buffer, size_t buffer_size, size_t *length_io, char ch);
int tool_buffer_append_text_checked(char *buffer, size_t buffer_size, size_t *length_io, const char *text);

#endif