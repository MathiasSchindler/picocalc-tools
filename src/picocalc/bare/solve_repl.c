#include "picocalc_lcd_bare.h"
#include "runtime.h"

#define REPL_COLS 40
#define REPL_ROWS 22
#define REPL_SCALE 1
#define REPL_X 4
#define REPL_Y 4
#define REPL_CELL_W (8 * REPL_SCALE)
#define REPL_CELL_H (14 * REPL_SCALE)
#define REPL_LINE_CAP 192U

int solve_main(int argc, char **argv);
void solve_repl_input_init(void);
int solve_repl_read_key(void);
void solve_repl_mirror_write(const char *data, size_t count);

static int g_repl_col;
static int g_repl_row;

__attribute__((weak)) void solve_repl_input_init(void) {
}

__attribute__((weak)) int solve_repl_read_key(void) {
    return -1;
}

__attribute__((weak)) void solve_repl_mirror_write(const char *data, size_t count) {
    (void)data;
    (void)count;
}

#ifdef PICOCALC_BARE_SIM
const char *picocalc_lcd_sim_ppm_path(void) {
    return "build/sim/sim_solve_repl.ppm";
}

int picocalc_lcd_sim_flush_at_exit(void) {
    return 0;
}
#endif

static void repl_console_newline(void) {
    g_repl_col = 0;
    g_repl_row += 1;
    if (g_repl_row >= REPL_ROWS) {
        g_repl_row = 0;
        picocalc_lcd_clear(0x000000u);
    }
}

static void repl_draw_cell(int col, int row, char ch) {
    char text[2];
    text[0] = ch;
    text[1] = 0;
    picocalc_lcd_puts_scale(REPL_X + col * REPL_CELL_W,
                            REPL_Y + row * REPL_CELL_H,
                            text,
                            0x00ff00u,
                            0x000000u,
                            REPL_SCALE);
}

static void repl_console_putchar(int ch) {
    if (ch == '\r') {
        g_repl_col = 0;
        return;
    }
    if (ch == '\n') {
        repl_console_newline();
        return;
    }
    if (ch < 32 || ch >= 127) return;
    if (g_repl_col >= REPL_COLS) repl_console_newline();
    repl_draw_cell(g_repl_col, g_repl_row, (char)ch);
    g_repl_col += 1;
}

static void repl_console_backspace(void) {
    if (g_repl_col > 0) {
        g_repl_col -= 1;
    } else if (g_repl_row > 0) {
        g_repl_row -= 1;
        g_repl_col = REPL_COLS - 1;
    } else {
        return;
    }
    repl_draw_cell(g_repl_col, g_repl_row, ' ');
}

long solve_platform_write(int fd, const void *data, size_t count) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t offset;
    (void)fd;
    for (offset = 0U; offset < count; ++offset) {
        repl_console_putchar(bytes[offset]);
    }
    solve_repl_mirror_write((const char *)data, count);
    return (long)count;
}

static int repl_is_enter(int key) {
    return key == '\n' || key == '\r';
}

static int repl_is_backspace(int key) {
    return key == 8 || key == 127;
}

static int repl_is_exit_line(const char *line) {
    return rt_strcmp(line, "exit") == 0 || rt_strcmp(line, "quit") == 0;
}

static void repl_run_solver(char *line) {
    char *argv[2];
    argv[0] = "solve";
    argv[1] = line;
    (void)solve_main(2, argv);
}

static void repl_prompt(void) {
    (void)rt_write_cstr(1, "solve> ");
}

void bare_main(void) {
    static char line[REPL_LINE_CAP];
    size_t line_length = 0U;

    picocalc_lcd_init();
    solve_repl_input_init();
    picocalc_lcd_clear(0x000000u);
    (void)rt_write_line(1, "PicoCalc solve");
    repl_prompt();

    while (1) {
        int key = solve_repl_read_key();
        if (key < 0 || key == 4) break;

        if (repl_is_enter(key)) {
            line[line_length] = '\0';
            repl_console_newline();
            if (line_length != 0U) {
                if (repl_is_exit_line(line)) break;
                repl_run_solver(line);
            }
            line_length = 0U;
            repl_prompt();
        } else if (repl_is_backspace(key)) {
            if (line_length > 0U) {
                line_length -= 1U;
                repl_console_backspace();
            }
        } else if (key >= 32 && key < 127) {
            if (line_length + 1U < sizeof(line)) {
                line[line_length] = (char)key;
                line_length += 1U;
                repl_console_putchar(key);
            }
        }
    }

#ifndef PICOCALC_BARE_SIM
    while (1) {
    }
#endif
}