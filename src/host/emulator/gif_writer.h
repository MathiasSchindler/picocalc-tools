#ifndef PICOCALC_EMULATOR_GIF_WRITER_H
#define PICOCALC_EMULATOR_GIF_WRITER_H

typedef unsigned long gif_usize;

typedef int (*GifWriteFn)(void *ctx, const void *data, gif_usize count);

typedef struct {
    GifWriteFn write_fn;
    void *ctx;
    unsigned int width;
    unsigned int height;
    unsigned int delay_cs;
    unsigned char block[255];
    unsigned int block_count;
    unsigned int bit_buffer;
    unsigned int bit_count;
    int failed;
} GifWriter;

int gif_begin_rgb8(GifWriter *writer, GifWriteFn write_fn, void *ctx, unsigned int width, unsigned int height, unsigned int fps);
int gif_write_frame_rgb8(GifWriter *writer, const unsigned char *rgb);
int gif_end(GifWriter *writer);

#endif