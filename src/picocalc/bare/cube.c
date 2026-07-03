#include "picocalc_kbd_bare.h"
#include "picocalc_lcd_bare.h"
#include "rp2040_regs.h"

#define LCD_W 320
#define LCD_H 320
#define FB_W 128
#define FB_H 128
#define FB_X 96
#define FB_Y 82
#define PROJ_SCALE 120
#define PROJ_DIST 360
#define KEY_POLL_PERIOD 4u
#define VERT_COUNT 8
#define FACE_COUNT 6
#define EDGE_COUNT 12

#define TIMERAWL REG32(0x40054028u)

typedef struct {
    int x;
    int y;
    int z;
} Point3;

typedef struct {
    int x;
    int y;
    int z;
} Point2;

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} Color;

static unsigned char g_fb[FB_W * FB_H * 3];

static const Point3 g_cube[VERT_COUNT] = {
    {-70, -70, -70}, { 70, -70, -70}, { 70,  70, -70}, {-70,  70, -70},
    {-70, -70,  70}, { 70, -70,  70}, { 70,  70,  70}, {-70,  70,  70}
};

static const unsigned char g_faces[FACE_COUNT][4] = {
    {0, 1, 2, 3}, {4, 7, 6, 5}, {0, 4, 5, 1},
    {3, 2, 6, 7}, {1, 5, 6, 2}, {0, 3, 7, 4}
};

static const unsigned char g_edges[EDGE_COUNT][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
    {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}
};

static const Color g_face_colors[FACE_COUNT] = {
    {230,  72,  58}, { 62, 174, 255}, { 70, 220, 130},
    {250, 210,  72}, {190, 104, 255}, { 52, 232, 218}
};

static const int g_sin_q12[65] = {
       0,  101,  201,  301,  401,  501,  601,  700,
     799,  897,  995, 1092, 1189, 1285, 1380, 1474,
    1567, 1660, 1751, 1842, 1931, 2019, 2106, 2191,
    2276, 2359, 2440, 2520, 2598, 2675, 2751, 2824,
    2896, 2967, 3035, 3102, 3166, 3229, 3290, 3349,
    3406, 3461, 3513, 3564, 3612, 3659, 3703, 3745,
    3784, 3822, 3857, 3889, 3920, 3948, 3973, 3996,
    4017, 4036, 4052, 4065, 4076, 4085, 4091, 4095,
    4096
};

static int abs_i(int value) {
    return value < 0 ? -value : value;
}

static int sin_q12(unsigned int phase) {
    int x = (int)(phase & 255u);
    if (x < 64) return g_sin_q12[x];
    if (x < 128) return g_sin_q12[128 - x];
    if (x < 192) return -g_sin_q12[x - 128];
    return -g_sin_q12[256 - x];
}

static int cos_q12(unsigned int phase) {
    return sin_q12(phase + 64u);
}

static void fb_clear(void) {
    int y;
    int x;
    unsigned char *p = g_fb;
    for (y = 0; y < FB_H; ++y) {
        unsigned char shade = (unsigned char)(8 + y / 12);
        for (x = 0; x < FB_W; ++x) {
            *p++ = (unsigned char)(shade / 2);
            *p++ = shade;
            *p++ = (unsigned char)(shade + 10);
        }
    }
}

static void fb_pixel_unchecked(int x, int y, Color color) {
    unsigned char *p = &g_fb[((y * FB_W) + x) * 3];
    p[0] = color.r;
    p[1] = color.g;
    p[2] = color.b;
}

static void fb_hline(int x0, int x1, int y, Color color) {
    int x;
    if (y < 0 || y >= FB_H) return;
    if (x0 > x1) {
        int tmp = x0;
        x0 = x1;
        x1 = tmp;
    }
    if (x0 < 0) x0 = 0;
    if (x1 >= FB_W) x1 = FB_W - 1;
    for (x = x0; x <= x1; ++x) fb_pixel_unchecked(x, y, color);
}

static void fb_dot(int x, int y, Color color, int radius) {
    int oy;
    int ox;
    for (oy = -radius; oy <= radius; ++oy) {
        int py = y + oy;
        if (py < 0 || py >= FB_H) continue;
        for (ox = -radius; ox <= radius; ++ox) {
            int px = x + ox;
            if (px >= 0 && px < FB_W) fb_pixel_unchecked(px, py, color);
        }
    }
}

static void fb_line(int x0, int y0, int x1, int y1, Color color, int thick) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int steps = abs_i(dx);
    int i;
    if (abs_i(dy) > steps) steps = abs_i(dy);
    if (steps < 1) steps = 1;
    if (steps > 48) steps = 48;
    for (i = 0; i <= steps; ++i) {
        int x = x0 + dx * i / steps;
        int y = y0 + dy * i / steps;
        fb_dot(x, y, color, thick);
    }
}

static void fill_triangle(Point2 a, Point2 b, Point2 c, Color color) {
    int y;
    int min_y = a.y;
    int max_y = a.y;
    if (b.y < min_y) min_y = b.y;
    if (c.y < min_y) min_y = c.y;
    if (b.y > max_y) max_y = b.y;
    if (c.y > max_y) max_y = c.y;
    if (min_y < 0) min_y = 0;
    if (max_y >= FB_H) max_y = FB_H - 1;
    for (y = min_y; y <= max_y; ++y) {
        int xs[3];
        int count = 0;
        Point2 p[3];
        int i;
        p[0] = a;
        p[1] = b;
        p[2] = c;
        for (i = 0; i < 3; ++i) {
            Point2 p0 = p[i];
            Point2 p1 = p[(i + 1) % 3];
            if ((y >= p0.y && y < p1.y) || (y >= p1.y && y < p0.y)) {
                int dy = p1.y - p0.y;
                int dx = p1.x - p0.x;
                xs[count++] = p0.x + (y - p0.y) * dx / dy;
            }
        }
        if (count == 2) fb_hline(xs[0], xs[1], y, color);
    }
}

static Color shade_color(Color base, int face_z, int mode) {
    int level;
    Color out;
    if (mode == 1) {
        base.r = 70;
        base.g = 190;
        base.b = 245;
    }
    level = 170 - face_z / 3;
    if (level < 88) level = 88;
    if (level > 255) level = 255;
    out.r = (unsigned char)((int)base.r * level / 255);
    out.g = (unsigned char)((int)base.g * level / 255);
    out.b = (unsigned char)((int)base.b * level / 255);
    return out;
}

static void project_cube(Point2 out[VERT_COUNT], unsigned int ax, unsigned int ay, unsigned int az) {
    int sx = sin_q12(ax);
    int cx = cos_q12(ax);
    int sy = sin_q12(ay);
    int cy = cos_q12(ay);
    int sz = sin_q12(az);
    int cz = cos_q12(az);
    int i;
    for (i = 0; i < VERT_COUNT; ++i) {
        int x = g_cube[i].x;
        int y = g_cube[i].y;
        int z = g_cube[i].z;
        int x1 = (x * cy + z * sy) >> 12;
        int z1 = (-x * sy + z * cy) >> 12;
        int y1 = (y * cx - z1 * sx) >> 12;
        int z2 = (y * sx + z1 * cx) >> 12;
        int x2 = (x1 * cz - y1 * sz) >> 12;
        int y2 = (x1 * sz + y1 * cz) >> 12;
        int den = z2 + PROJ_DIST;
        out[i].x = FB_W / 2 + (x2 * PROJ_SCALE) / den;
        out[i].y = FB_H / 2 - (y2 * PROJ_SCALE) / den;
        out[i].z = z2;
    }
}

static void draw_face(Point2 v[VERT_COUNT], int face_index, Color color) {
    Point2 a = v[g_faces[face_index][0]];
    Point2 b = v[g_faces[face_index][1]];
    Point2 c = v[g_faces[face_index][2]];
    Point2 d = v[g_faces[face_index][3]];
    fill_triangle(a, b, c, color);
    fill_triangle(a, c, d, color);
}

static int face_depth(Point2 v[VERT_COUNT], int face_index) {
    return (v[g_faces[face_index][0]].z + v[g_faces[face_index][1]].z +
            v[g_faces[face_index][2]].z + v[g_faces[face_index][3]].z) / 4;
}

static void draw_cube(Point2 v[VERT_COUNT], int mode) {
    Color edge = {250, 252, 255};
    Color near_edge = {255, 244, 120};
    Color far_edge = {45, 150, 255};
    Color side_edge = {255, 92, 92};
    Color vertex = {255, 255, 255};
    int order[FACE_COUNT] = {0, 1, 2, 3, 4, 5};
    int i;
    if (mode != 0) {
        int a;
        for (a = 0; a < FACE_COUNT - 1; ++a) {
            int b;
            for (b = a + 1; b < FACE_COUNT; ++b) {
                if (face_depth(v, order[a]) < face_depth(v, order[b])) {
                    int tmp = order[a];
                    order[a] = order[b];
                    order[b] = tmp;
                }
            }
        }
        for (i = 0; i < FACE_COUNT; ++i) {
            int face = order[i];
            draw_face(v, face, shade_color(g_face_colors[face], face_depth(v, face), mode));
        }
    }
    if (mode == 0) {
        Color soft = {40, 95, 140};
        for (i = 0; i < EDGE_COUNT; ++i) {
            Point2 a = v[g_edges[i][0]];
            Point2 b = v[g_edges[i][1]];
            fb_line(a.x, a.y, b.x, b.y, soft, 1);
        }
    }
    for (i = 0; i < EDGE_COUNT; ++i) {
        Point2 a = v[g_edges[i][0]];
        Point2 b = v[g_edges[i][1]];
        Color color = edge;
        if (mode == 0) {
            if (i < 4) color = far_edge;
            else if (i < 8) color = near_edge;
            else color = side_edge;
        }
        fb_line(a.x, a.y, b.x, b.y, color, 0);
    }
    if (mode == 0) {
        for (i = 0; i < VERT_COUNT; ++i) fb_dot(v[i].x, v[i].y, vertex, 1);
    }
}

static void append_char(char *buf, int *pos, char ch) {
    buf[*pos] = ch;
    *pos += 1;
}

static void append_text(char *buf, int *pos, const char *text) {
    while (*text != 0) append_char(buf, pos, *text++);
}

static void append_uint(char *buf, int *pos, unsigned int value) {
    char tmp[10];
    int n = 0;
    if (value == 0u) {
        append_char(buf, pos, '0');
        return;
    }
    while (value != 0u && n < (int)sizeof(tmp)) {
        tmp[n++] = (char)('0' + value % 10u);
        value /= 10u;
    }
    while (n-- > 0) append_char(buf, pos, tmp[n]);
}

static void append_hex2(char *buf, int *pos, int value) {
    static const char hex[] = "0123456789ABCDEF";
    if (value < 0) {
        append_text(buf, pos, "--");
        return;
    }
    append_char(buf, pos, hex[((unsigned int)value >> 4) & 0xfu]);
    append_char(buf, pos, hex[(unsigned int)value & 0xfu]);
}

static void draw_status(int mode, int speed, unsigned int fps, int last_key, unsigned int frame) {
    static const char *names[3] = {"WIRE", "SOLID", "COLOR"};
    char left[32];
    char right[16];
    int pos = 0;
    picocalc_lcd_fill_rect(0, 0, LCD_W - 1, 24, 0x000000u);
    append_text(left, &pos, names[mode]);
    append_text(left, &pos, " SPD ");
    append_uint(left, &pos, (unsigned int)speed);
    append_text(left, &pos, " KEY ");
    append_hex2(left, &pos, last_key);
    left[pos] = 0;
    pos = 0;
    append_uint(right, &pos, fps);
    append_text(right, &pos, " FPS");
    right[pos] = 0;
    picocalc_lcd_puts_scale(6, 5, left, 0x80ffb0u, 0x000000u, 1);
    picocalc_lcd_puts_scale(248, 5, right, 0x80ffb0u, 0x000000u, 1);
    picocalc_lcd_fill_rect(154 + (int)(frame & 31u), 10, 158 + (int)(frame & 31u), 14, 0xffd050u);
}

static void frame_delay(void) {
    volatile unsigned int i;
    for (i = 0; i < 3500u; ++i) {
    }
}

void bare_main(void) {
    unsigned int ax = 33;
    unsigned int ay = 51;
    unsigned int az = 19;
    unsigned int last_us;
    unsigned int frame = 0;
    unsigned int fps = 0;
    int mode = 0;
    int speed = 6;
    int key_cooldown = 0;
    int key = -1;
    int last_key = -1;
    Point2 projected[VERT_COUNT];

    picocalc_lcd_init();
    picocalc_kbd_init();
    picocalc_lcd_clear(0x000000u);
    picocalc_lcd_fill_rect(FB_X - 2, FB_Y - 2, FB_X + FB_W + 1, FB_Y + FB_H + 1, 0x103040u);
    picocalc_lcd_puts_scale(9, 286, "ARROWS mode/speed  WASD also", 0x808080u, 0x000000u, 1);
    last_us = TIMERAWL;

    while (1) {
        unsigned int now;
        unsigned int elapsed;
        if (key_cooldown > 0) key_cooldown -= 1;
        if (key >= 0 && key_cooldown == 0) {
            if (key == PICOCALC_KEY_RIGHT) mode = (mode + 1) % 3;
            else if (key == PICOCALC_KEY_LEFT) mode = (mode + 2) % 3;
            else if (key == PICOCALC_KEY_UP && speed < 12) speed += 1;
            else if (key == PICOCALC_KEY_DOWN && speed > 1) speed -= 1;
            else if (key == 'd' || key == 'D') mode = (mode + 1) % 3;
            else if (key == 'a' || key == 'A') mode = (mode + 2) % 3;
            else if ((key == 'w' || key == 'W') && speed < 12) speed += 1;
            else if ((key == 's' || key == 'S') && speed > 1) speed -= 1;
            key_cooldown = 5;
            key = -1;
        }

        ax += (unsigned int)speed;
        ay += (unsigned int)(speed + 1);
        az += (unsigned int)((speed + 2) / 2);
        fb_clear();
        project_cube(projected, ax, ay, az);
        draw_cube(projected, mode);
        picocalc_lcd_blit_rgb(FB_X, FB_Y, FB_W, FB_H, g_fb);

        now = TIMERAWL;
        elapsed = now - last_us;
        last_us = now;
        if (elapsed != 0u) fps = 1000000u / elapsed;
        if ((frame & 7u) == 0u) draw_status(mode, speed, fps, last_key, frame >> 3);
        if ((frame % KEY_POLL_PERIOD) == KEY_POLL_PERIOD - 1u) {
            key = picocalc_kbd_read_key();
            if (key >= 0) last_key = key;
        }
        frame += 1u;
        frame_delay();
    }
}