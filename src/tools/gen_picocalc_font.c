#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fontrender/font_backend.h"
#include "fontrender/fr_platform.h"

#define FIRST_GLYPH 32
#define LAST_GLYPH 126
#define FONT_SIZE 12
#define CELL_W 8
#define CELL_H 14
#define CELL_ROW_BYTES ((CELL_W + 1) / 2)
#define GLYPH_COUNT (LAST_GLYPH - FIRST_GLYPH + 1)

static unsigned char g_alpha[GLYPH_COUNT][CELL_H][CELL_ROW_BYTES];

static void *host_alloc(void *user_data, size_t size) {
    (void)user_data;
    return malloc(size == 0 ? 1 : size);
}

static void *host_realloc(void *user_data, void *ptr, size_t size) {
    (void)user_data;
    return realloc(ptr, size == 0 ? 1 : size);
}

static void host_free(void *user_data, void *ptr) {
    (void)user_data;
    free(ptr);
}

static void *host_memcpy(void *user_data, void *dst, const void *src, size_t size) {
    (void)user_data;
    return memcpy(dst, src, size);
}

static void *host_memmove(void *user_data, void *dst, const void *src, size_t size) {
    (void)user_data;
    return memmove(dst, src, size);
}

static void *host_memset(void *user_data, void *dst, int value, size_t size) {
    (void)user_data;
    return memset(dst, value, size);
}

static int host_memcmp(void *user_data, const void *lhs, const void *rhs, size_t size) {
    (void)user_data;
    return memcmp(lhs, rhs, size);
}

static FrPlatformFileResult host_load_file(void *user_data, const char *path, FrPlatformFile *out_file) {
    FILE *file;
    long size;
    unsigned char *data;
    (void)user_data;
    out_file->data = NULL;
    out_file->size = 0;
    out_file->handle = NULL;
    file = fopen(path, "rb");
    if (file == NULL) return FR_PLATFORM_FILE_ERR_IO;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return FR_PLATFORM_FILE_ERR_IO;
    }
    size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return FR_PLATFORM_FILE_ERR_IO;
    }
    data = (unsigned char *)malloc((size_t)size == 0 ? 1 : (size_t)size);
    if (data == NULL) {
        fclose(file);
        return FR_PLATFORM_FILE_ERR_NO_MEMORY;
    }
    if (fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return FR_PLATFORM_FILE_ERR_IO;
    }
    fclose(file);
    out_file->data = data;
    out_file->size = (size_t)size;
    out_file->handle = data;
    return FR_PLATFORM_FILE_OK;
}

static void host_unload_file(void *user_data, FrPlatformFile *file) {
    (void)user_data;
    free(file->handle);
    file->data = NULL;
    file->size = 0;
    file->handle = NULL;
}

static void host_log(void *user_data, FrPlatformLogLevel level, const char *component, const char *fmt, va_list args) {
    (void)user_data;
    (void)level;
    if (component != NULL) fprintf(stderr, "fontrender:%s: ", component);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
}

static int install_platform(void) {
    FrPlatform platform;
    platform.user_data = NULL;
    platform.alloc = host_alloc;
    platform.realloc = host_realloc;
    platform.free = host_free;
    platform.memcpy = host_memcpy;
    platform.memmove = host_memmove;
    platform.memset = host_memset;
    platform.memcmp = host_memcmp;
    platform.load_file = host_load_file;
    platform.unload_file = host_unload_file;
    platform.mutex_create = NULL;
    platform.mutex_destroy = NULL;
    platform.mutex_lock = NULL;
    platform.mutex_unlock = NULL;
    platform.log = host_log;
    return fr_platform_set(&platform);
}

static void put_pixel(unsigned char *ppm, int width, int x, int y, unsigned char value) {
    size_t off = ((size_t)y * (size_t)width + (size_t)x) * 3u;
    ppm[off + 0] = value;
    ppm[off + 1] = value;
    ppm[off + 2] = value;
}

static void emit_preview(const char *path) {
    enum { SCALE = 4, COLS = 19, ROWS = 5, PAD = 2 };
    int width = COLS * (CELL_W + PAD) * SCALE;
    int height = ROWS * (CELL_H + PAD) * SCALE;
    size_t bytes = (size_t)width * (size_t)height * 3u;
    unsigned char *ppm = (unsigned char *)malloc(bytes);
    FILE *file;
    int glyph;
    memset(ppm, 24, bytes);
    for (glyph = 0; glyph < GLYPH_COUNT; ++glyph) {
        int gx = glyph % COLS;
        int gy = glyph / COLS;
        int row;
        for (row = 0; row < CELL_H; ++row) {
            int col;
            for (col = 0; col < CELL_W; ++col) {
                unsigned char packed = g_alpha[glyph][row][col >> 1];
                unsigned char alpha = (col & 1) == 0 ? (unsigned char)(packed >> 4) : (unsigned char)(packed & 15u);
                unsigned char value = (unsigned char)(24u + (unsigned int)alpha * 206u / 15u);
                int px0 = (gx * (CELL_W + PAD) + col) * SCALE;
                int py0 = (gy * (CELL_H + PAD) + row) * SCALE;
                int sx;
                int sy;
                for (sy = 0; sy < SCALE; ++sy) {
                    for (sx = 0; sx < SCALE; ++sx) {
                        put_pixel(ppm, width, px0 + sx, py0 + sy, value);
                    }
                }
            }
        }
    }
    file = fopen(path, "wb");
    if (file != NULL) {
        fprintf(file, "P6\n%d %d\n255\n", width, height);
        fwrite(ppm, 1, bytes, file);
        fclose(file);
    }
    free(ppm);
}

static void render_font(FrFont *font) {
    int line_height = fr_font_line_height(font, FONT_SIZE);
    int baseline = line_height - 3;
    int code;
    if (baseline < 9) baseline = 10;
    if (baseline >= CELL_H) baseline = CELL_H - 2;
    for (code = FIRST_GLYPH; code <= LAST_GLYPH; ++code) {
        const FrGlyph *glyph = fr_font_get_glyph(font, (uint32_t)code, FONT_SIZE, 0);
        int index = code - FIRST_GLYPH;
        int x0;
        int y0;
        int y;
        if (glyph == NULL || glyph->bitmap == NULL) continue;
        x0 = (CELL_W - glyph->advance) / 2 + glyph->left;
        y0 = baseline - glyph->top;
        for (y = 0; y < glyph->height; ++y) {
            int x;
            int dst_y = y0 + y;
            if (dst_y < 0 || dst_y >= CELL_H) continue;
            for (x = 0; x < glyph->width; ++x) {
                int dst_x = x0 + x;
                if (dst_x >= 0 && dst_x < CELL_W) {
                    unsigned int alpha = ((unsigned int)glyph->bitmap[y * glyph->width + x] + 8u) / 17u;
                    unsigned char *packed = &g_alpha[index][dst_y][dst_x >> 1];
                    if (alpha > 15u) alpha = 15u;
                    if (alpha != 0u) {
                        if ((dst_x & 1) == 0) *packed = (unsigned char)((*packed & 0x0fu) | (alpha << 4));
                        else *packed = (unsigned char)((*packed & 0xf0u) | alpha);
                    }
                }
            }
        }
    }
}

static int write_header(const char *path) {
    FILE *file = fopen(path, "w");
    int glyph;
    if (file == NULL) return -1;
    fprintf(file, "#ifndef PICOCALC_CASCADIA_8X14_H\n");
    fprintf(file, "#define PICOCALC_CASCADIA_8X14_H\n\n");
    fprintf(file, "#define PICOCALC_CASCADIA_FIRST 32\n");
    fprintf(file, "#define PICOCALC_CASCADIA_LAST 126\n");
    fprintf(file, "#define PICOCALC_CASCADIA_WIDTH 8\n");
    fprintf(file, "#define PICOCALC_CASCADIA_HEIGHT 14\n\n");
    fprintf(file, "#define PICOCALC_CASCADIA_ROW_BYTES 4\n");
    fprintf(file, "#define PICOCALC_CASCADIA_BPP 4\n\n");
    fprintf(file, "static const unsigned char picocalc_cascadia_8x14[%d][%d][%d] = {\n", GLYPH_COUNT, CELL_H, CELL_ROW_BYTES);
    for (glyph = 0; glyph < GLYPH_COUNT; ++glyph) {
        int row;
        fprintf(file, "    {\n");
        for (row = 0; row < CELL_H; ++row) {
            int byte;
            fprintf(file, "        {");
            for (byte = 0; byte < CELL_ROW_BYTES; ++byte) {
                fprintf(file, "0x%02x%s", g_alpha[glyph][row][byte], byte + 1 == CELL_ROW_BYTES ? "" : ", ");
            }
            fprintf(file, "}%s\n", row + 1 == CELL_H ? "" : ",");
        }
        fprintf(file, "    }%s /* %c */\n", glyph + 1 == GLYPH_COUNT ? "" : ",", glyph + FIRST_GLYPH == '\\' ? '/' : glyph + FIRST_GLYPH);
    }
    fprintf(file, "};\n\n#endif\n");
    fclose(file);
    return 0;
}

int main(int argc, char **argv) {
    FrFont *font;
    const char *font_path = argc > 1 ? argv[1] : "vendor/microsoft/cascadia/CascadiaMono.ttf";
    const char *header_path = argc > 2 ? argv[2] : "build/font/picocalc_cascadia_8x14.h";
    const char *preview_path = argc > 3 ? argv[3] : "build/font/picocalc_cascadia_8x14.ppm";
    if (install_platform() != 0) {
        fprintf(stderr, "failed to install fontrender platform\n");
        return 1;
    }
    if (fr_font_open(&font, font_path) != 0) {
        fprintf(stderr, "failed to open font: %s\n", font_path);
        return 1;
    }
    render_font(font);
    fr_font_close(font);
    if (write_header(header_path) != 0) {
        fprintf(stderr, "failed to write %s\n", header_path);
        return 1;
    }
    emit_preview(preview_path);
    printf("wrote %s and %s\n", header_path, preview_path);
    return 0;
}