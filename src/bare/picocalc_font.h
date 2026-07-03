#ifndef PICOCALC_FONT_H
#define PICOCALC_FONT_H

#include "picocalc_cascadia_8x14.h"

#define PICOCALC_FONT_CELL_W PICOCALC_CASCADIA_WIDTH
#define PICOCALC_FONT_CELL_H PICOCALC_CASCADIA_HEIGHT
#define PICOCALC_FONT_ROW_BYTES PICOCALC_CASCADIA_ROW_BYTES

static const unsigned char *picocalc_font_glyph(char ch) {
    unsigned int code = (unsigned char)ch;
    if (code < PICOCALC_CASCADIA_FIRST || code > PICOCALC_CASCADIA_LAST) {
        code = ' ';
    }
    return &picocalc_cascadia_8x14[code - PICOCALC_CASCADIA_FIRST][0][0];
}

static unsigned int picocalc_font_alpha(const unsigned char *glyph, int row, int col) {
    unsigned char packed = glyph[row * PICOCALC_FONT_ROW_BYTES + (col >> 1)];
    if ((col & 1) == 0) return (unsigned int)(packed >> 4);
    return (unsigned int)(packed & 15u);
}

#endif