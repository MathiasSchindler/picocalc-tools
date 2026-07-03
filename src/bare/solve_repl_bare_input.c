#include "picocalc_kbd_bare.h"

void solve_repl_input_init(void) {
    picocalc_kbd_init();
}

int solve_repl_read_key(void) {
    while (1) {
        int key = picocalc_kbd_read_key();
        if (key >= 0) return key;
    }
}