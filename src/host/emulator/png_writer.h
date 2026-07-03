#ifndef PICOCALC_EMULATOR_PNG_WRITER_H
#define PICOCALC_EMULATOR_PNG_WRITER_H

typedef unsigned long png_usize;

typedef int (*PngWriteFn)(void *ctx, const void *data, png_usize count);

#define PNG_RGB8_FILTERED_SIZE(width, height) (((png_usize)(width) * 3u + 1u) * (png_usize)(height))
#define PNG_ZLIB_FIXED_RLE_BOUND(size) ((((png_usize)(size) * 9u + 7u) / 8u) + 16u)
#define PNG_RGB8_WORK_SIZE(width, height) (PNG_RGB8_FILTERED_SIZE((width), (height)) + PNG_ZLIB_FIXED_RLE_BOUND(PNG_RGB8_FILTERED_SIZE((width), (height))))

int png_write_rgb8(PngWriteFn write_fn, void *ctx, const unsigned char *rgb, unsigned int width, unsigned int height, unsigned char *work, png_usize work_capacity);

#endif
