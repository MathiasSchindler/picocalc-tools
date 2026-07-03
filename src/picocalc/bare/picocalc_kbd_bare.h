#ifndef PICOCALC_KBD_BARE_H
#define PICOCALC_KBD_BARE_H

#define PICOCALC_KEY_ALT        0xa1
#define PICOCALC_KEY_SHIFT      0xa2
#define PICOCALC_KEY_CTRL       0xa5

#define PICOCALC_KEY_LEFT       0xb4
#define PICOCALC_KEY_UP         0xb5
#define PICOCALC_KEY_DOWN       0xb6
#define PICOCALC_KEY_RIGHT      0xb7

void picocalc_kbd_init(void);
int picocalc_kbd_read_key(void);

#endif