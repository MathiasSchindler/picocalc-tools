#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UF2_MAGIC_START0 0x0a324655u
#define UF2_MAGIC_START1 0x9e5d5157u
#define UF2_MAGIC_END 0x0ab16f30u
#define UF2_FLAG_FAMILY_ID_PRESENT 0x00002000u
#define UF2_RP2040_FAMILY_ID 0xe48bff56u
#define UF2_BLOCK_SIZE 512u
#define UF2_PAYLOAD_SIZE 256u
#define RP2040_FLASH_BASE 0x10000000u
#define DEFAULT_APP_BASE 0x10032000u
#define DEFAULT_FLASH_APP_BASE 0x10000100u
#define WRAPPER_VECTOR_BLOCK_OFFSET 0x100u
#define WRAPPER_VECTOR_COUNT 48u
#define WRAPPER_ENTRY_OFFSET (WRAPPER_VECTOR_COUNT * 4u)
#define WRAPPER_LITERAL_APP_OFFSET (WRAPPER_ENTRY_OFFSET + 20u)
#define WRAPPER_LITERAL_VTOR_OFFSET (WRAPPER_ENTRY_OFFSET + 24u)

static const unsigned char g_wrapper_code[] = {
    0x72, 0xb6,             /* cpsid i */
    0x04, 0x49,             /* ldr r1, [pc, #16] */
    0x04, 0x48,             /* ldr r0, [pc, #16] */
    0x01, 0x60,             /* str r1, [r0] */
    0x08, 0x68,             /* ldr r0, [r1] */
    0x80, 0xf3, 0x08, 0x88, /* msr msp, r0 */
    0x48, 0x68,             /* ldr r0, [r1, #4] */
    0x00, 0x47,             /* bx r0 */
    0x00, 0xbf,             /* nop; align literals */
    0x00, 0x20, 0x03, 0x10, /* .word 0x10032000 */
    0x08, 0xed, 0x00, 0xe0, /* .word 0xe000ed08 */
};

static void put32le(unsigned char *dst, uint32_t value) {
    dst[0] = (unsigned char)(value & 0xffu);
    dst[1] = (unsigned char)((value >> 8) & 0xffu);
    dst[2] = (unsigned char)((value >> 16) & 0xffu);
    dst[3] = (unsigned char)((value >> 24) & 0xffu);
}

static uint32_t get32le(const unsigned char *src) {
    return (uint32_t)src[0]
        | ((uint32_t)src[1] << 8)
        | ((uint32_t)src[2] << 16)
        | ((uint32_t)src[3] << 24);
}

static int parse_u32(const char *text, uint32_t *out_value) {
    char *end = NULL;
    unsigned long value;
    errno = 0;
    value = strtoul(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || value > 0xfffffffful) return 0;
    *out_value = (uint32_t)value;
    return 1;
}

static int file_size(FILE *file, long *out_size) {
    long size;
    if (fseek(file, 0, SEEK_END) != 0) return 0;
    size = ftell(file);
    if (size < 0) return 0;
    if (fseek(file, 0, SEEK_SET) != 0) return 0;
    *out_size = size;
    return 1;
}

static unsigned char *load_file(const char *path, uint32_t *out_size) {
    FILE *file;
    long size_long = 0;
    unsigned char *data;
    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "bin2uf2: cannot open %s: %s\n", path, strerror(errno));
        return NULL;
    }
    if (!file_size(file, &size_long) || size_long <= 0 || size_long > 0x01000000L) {
        fprintf(stderr, "bin2uf2: unsupported size for %s: %ld\n", path, size_long);
        fclose(file);
        return NULL;
    }
    data = (unsigned char *)malloc((size_t)size_long);
    if (data == NULL) {
        fprintf(stderr, "bin2uf2: out of memory reading %s\n", path);
        fclose(file);
        return NULL;
    }
    if (fread(data, 1, (size_t)size_long, file) != (size_t)size_long) {
        fprintf(stderr, "bin2uf2: read failed from %s\n", path);
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *out_size = (uint32_t)size_long;
    return data;
}

static int load_boot2_from_uf2(const char *path, unsigned char *boot2) {
    unsigned char block[UF2_BLOCK_SIZE];
    FILE *file = fopen(path, "rb");
    uint32_t payload_size;
    if (file == NULL) {
        fprintf(stderr, "bin2uf2: cannot open boot2 source %s: %s\n", path, strerror(errno));
        return 0;
    }
    if (fread(block, 1, sizeof(block), file) != sizeof(block)) {
        fprintf(stderr, "bin2uf2: cannot read first UF2 block from %s\n", path);
        fclose(file);
        return 0;
    }
    fclose(file);
    payload_size = get32le(block + 16);
    if (get32le(block + 0) != UF2_MAGIC_START0 ||
        get32le(block + 4) != UF2_MAGIC_START1 ||
        get32le(block + 12) != RP2040_FLASH_BASE ||
        payload_size < UF2_PAYLOAD_SIZE ||
        get32le(block + UF2_BLOCK_SIZE - 4u) != UF2_MAGIC_END) {
        fprintf(stderr, "bin2uf2: %s does not look like an RP2040 flash-start UF2\n", path);
        return 0;
    }
    memcpy(boot2, block + 32, UF2_PAYLOAD_SIZE);
    return 1;
}

static int write_uf2_block(FILE *output, uint32_t target_addr, uint32_t block_no, uint32_t blocks, uint32_t family_id, const unsigned char *payload) {
    unsigned char block[UF2_BLOCK_SIZE];
    memset(block, 0, sizeof(block));
    put32le(block + 0, UF2_MAGIC_START0);
    put32le(block + 4, UF2_MAGIC_START1);
    put32le(block + 8, UF2_FLAG_FAMILY_ID_PRESENT);
    put32le(block + 12, target_addr);
    put32le(block + 16, UF2_PAYLOAD_SIZE);
    put32le(block + 20, block_no);
    put32le(block + 24, blocks);
    put32le(block + 28, family_id);
    memcpy(block + 32, payload, UF2_PAYLOAD_SIZE);
    put32le(block + UF2_BLOCK_SIZE - 4u, UF2_MAGIC_END);
    return fwrite(block, 1, sizeof(block), output) == sizeof(block);
}

static int write_plain_uf2(const char *input_path, const char *output_path, uint32_t target_base, uint32_t family_id) {
    unsigned char *input_data;
    uint32_t input_size;
    uint32_t blocks;
    uint32_t block_no;
    FILE *output;

    input_data = load_file(input_path, &input_size);
    if (input_data == NULL) return 1;
    blocks = (input_size + UF2_PAYLOAD_SIZE - 1u) / UF2_PAYLOAD_SIZE;

    output = fopen(output_path, "wb");
    if (output == NULL) {
        fprintf(stderr, "bin2uf2: cannot open %s: %s\n", output_path, strerror(errno));
        free(input_data);
        return 1;
    }
    for (block_no = 0; block_no < blocks; ++block_no) {
        unsigned char payload[UF2_PAYLOAD_SIZE];
        uint32_t offset = block_no * UF2_PAYLOAD_SIZE;
        uint32_t remaining = input_size - offset;
        uint32_t count = remaining < UF2_PAYLOAD_SIZE ? remaining : UF2_PAYLOAD_SIZE;
        memset(payload, 0xff, sizeof(payload));
        memcpy(payload, input_data + offset, count);
        if (!write_uf2_block(output, target_base + offset, block_no, blocks, family_id, payload)) {
            fprintf(stderr, "bin2uf2: write failed to %s\n", output_path);
            fclose(output);
            free(input_data);
            return 1;
        }
    }
    if (fclose(output) != 0) {
        fprintf(stderr, "bin2uf2: close failed for %s: %s\n", output_path, strerror(errno));
        free(input_data);
        return 1;
    }
    printf("%s: %u bytes -> %u UF2 blocks at 0x%08x\n", output_path, input_size, blocks, target_base);
    free(input_data);
    return 0;
}

static int write_boot2_app_uf2(const char *boot2_path, const char *input_path, const char *output_path, uint32_t app_base, uint32_t family_id) {
    unsigned char boot2[UF2_PAYLOAD_SIZE];
    unsigned char *app_data;
    uint32_t app_size;
    uint32_t app_blocks;
    uint32_t blocks;
    uint32_t block_no;
    FILE *output;

    if (app_base != RP2040_FLASH_BASE + UF2_PAYLOAD_SIZE) {
        fprintf(stderr, "bin2uf2: boot2 app base must be 0x10000100 for compact flash-start UF2s\n");
        return 1;
    }
    if (!load_boot2_from_uf2(boot2_path, boot2)) return 1;
    app_data = load_file(input_path, &app_size);
    if (app_data == NULL) return 1;

    app_blocks = (app_size + UF2_PAYLOAD_SIZE - 1u) / UF2_PAYLOAD_SIZE;
    blocks = 1u + app_blocks;

    output = fopen(output_path, "wb");
    if (output == NULL) {
        fprintf(stderr, "bin2uf2: cannot open %s: %s\n", output_path, strerror(errno));
        free(app_data);
        return 1;
    }
    for (block_no = 0; block_no < blocks; ++block_no) {
        unsigned char payload[UF2_PAYLOAD_SIZE];
        uint32_t target_addr;
        memset(payload, 0xff, sizeof(payload));
        if (block_no == 0) {
            target_addr = RP2040_FLASH_BASE;
            memcpy(payload, boot2, sizeof(boot2));
        } else {
            uint32_t app_pos = (block_no - 1u) * UF2_PAYLOAD_SIZE;
            uint32_t remaining = app_size - app_pos;
            uint32_t count = remaining < UF2_PAYLOAD_SIZE ? remaining : UF2_PAYLOAD_SIZE;
            target_addr = app_base + app_pos;
            memcpy(payload, app_data + app_pos, count);
        }
        if (!write_uf2_block(output, target_addr, block_no, blocks, family_id, payload)) {
            fprintf(stderr, "bin2uf2: write failed to %s\n", output_path);
            fclose(output);
            free(app_data);
            return 1;
        }
    }
    if (fclose(output) != 0) {
        fprintf(stderr, "bin2uf2: close failed for %s: %s\n", output_path, strerror(errno));
        free(app_data);
        return 1;
    }
    printf("%s: boot2 + %u-byte app at 0x%08x -> %u UF2 blocks\n", output_path, app_size, app_base, blocks);
    free(app_data);
    return 0;
}

static int write_wrapped_uf2(const char *boot2_path, const char *input_path, const char *output_path, uint32_t app_base, uint32_t family_id) {
    unsigned char boot2[UF2_PAYLOAD_SIZE];
    unsigned char *app_data;
    uint32_t app_size;
    uint32_t app_blocks;
    uint32_t app_start_block;
    uint32_t blocks;
    uint32_t block_no;
    FILE *output;

    if (app_base < RP2040_FLASH_BASE + 2u * UF2_PAYLOAD_SIZE || ((app_base - RP2040_FLASH_BASE) % UF2_PAYLOAD_SIZE) != 0) {
        fprintf(stderr, "bin2uf2: wrapped app base must be 256-byte aligned after 0x10000200\n");
        return 1;
    }
    if (!load_boot2_from_uf2(boot2_path, boot2)) return 1;
    app_data = load_file(input_path, &app_size);
    if (app_data == NULL) return 1;

    app_blocks = (app_size + UF2_PAYLOAD_SIZE - 1u) / UF2_PAYLOAD_SIZE;
    app_start_block = (app_base - RP2040_FLASH_BASE) / UF2_PAYLOAD_SIZE;
    blocks = app_start_block + app_blocks;

    output = fopen(output_path, "wb");
    if (output == NULL) {
        fprintf(stderr, "bin2uf2: cannot open %s: %s\n", output_path, strerror(errno));
        free(app_data);
        return 1;
    }
    for (block_no = 0; block_no < blocks; ++block_no) {
        unsigned char payload[UF2_PAYLOAD_SIZE];
        uint32_t target_addr;
        memset(payload, 0xff, sizeof(payload));
        if (block_no == 0) {
            target_addr = RP2040_FLASH_BASE;
            memcpy(payload, boot2, sizeof(boot2));
        } else if (block_no == 1) {
            uint32_t wrapper_entry = RP2040_FLASH_BASE + WRAPPER_VECTOR_BLOCK_OFFSET + WRAPPER_ENTRY_OFFSET + 1u;
            target_addr = RP2040_FLASH_BASE + WRAPPER_VECTOR_BLOCK_OFFSET;
            put32le(payload + 0, 0x20042000u);
            for (uint32_t vector_index = 1u; vector_index < WRAPPER_VECTOR_COUNT; ++vector_index) {
                put32le(payload + vector_index * 4u, wrapper_entry);
            }
            memcpy(payload + WRAPPER_ENTRY_OFFSET, g_wrapper_code, sizeof(g_wrapper_code));
            put32le(payload + WRAPPER_LITERAL_APP_OFFSET, app_base);
            put32le(payload + WRAPPER_LITERAL_VTOR_OFFSET, 0xe000ed08u);
        } else if (block_no >= app_start_block) {
            uint32_t app_pos = (block_no - app_start_block) * UF2_PAYLOAD_SIZE;
            uint32_t remaining = app_size - app_pos;
            uint32_t count = remaining < UF2_PAYLOAD_SIZE ? remaining : UF2_PAYLOAD_SIZE;
            target_addr = app_base + app_pos;
            memcpy(payload, app_data + app_pos, count);
        } else {
            target_addr = RP2040_FLASH_BASE + block_no * UF2_PAYLOAD_SIZE;
        }
        if (!write_uf2_block(output, target_addr, block_no, blocks, family_id, payload)) {
            fprintf(stderr, "bin2uf2: write failed to %s\n", output_path);
            fclose(output);
            free(app_data);
            return 1;
        }
    }
    if (fclose(output) != 0) {
        fprintf(stderr, "bin2uf2: close failed for %s: %s\n", output_path, strerror(errno));
        free(app_data);
        return 1;
    }
    printf("%s: %u-byte app wrapped at 0x%08x -> %u contiguous UF2 blocks\n", output_path, app_size, app_base, blocks);
    free(app_data);
    return 0;
}

int main(int argc, char **argv) {
    uint32_t target_base = DEFAULT_APP_BASE;
    uint32_t family_id = UF2_RP2040_FAMILY_ID;

    if (argc >= 2 && strcmp(argv[1], "--flash-start") == 0) {
        if (argc < 5 || argc > 7) {
            fprintf(stderr, "usage: %s --flash-start boot2-source.uf2 input.bin output.uf2 [app-address] [family-id]\n", argv[0]);
            return 2;
        }
        if (argc >= 6 && !parse_u32(argv[5], &target_base)) {
            fprintf(stderr, "%s: invalid app address: %s\n", argv[0], argv[5]);
            return 2;
        }
        if (argc >= 7 && !parse_u32(argv[6], &family_id)) {
            fprintf(stderr, "%s: invalid family id: %s\n", argv[0], argv[6]);
            return 2;
        }
        return write_wrapped_uf2(argv[2], argv[3], argv[4], target_base, family_id);
    }

    if (argc >= 2 && strcmp(argv[1], "--boot2-app") == 0) {
        target_base = DEFAULT_FLASH_APP_BASE;
        if (argc < 5 || argc > 7) {
            fprintf(stderr, "usage: %s --boot2-app boot2-source.uf2 input.bin output.uf2 [app-address] [family-id]\n", argv[0]);
            return 2;
        }
        if (argc >= 6 && !parse_u32(argv[5], &target_base)) {
            fprintf(stderr, "%s: invalid app address: %s\n", argv[0], argv[5]);
            return 2;
        }
        if (argc >= 7 && !parse_u32(argv[6], &family_id)) {
            fprintf(stderr, "%s: invalid family id: %s\n", argv[0], argv[6]);
            return 2;
        }
        return write_boot2_app_uf2(argv[2], argv[3], argv[4], target_base, family_id);
    }

    if (argc < 3 || argc > 5) {
        fprintf(stderr, "usage: %s input.bin output.uf2 [target-address] [family-id]\n", argv[0]);
        fprintf(stderr, "       %s --flash-start boot2-source.uf2 input.bin output.uf2 [app-address] [family-id]\n", argv[0]);
        fprintf(stderr, "       %s --boot2-app boot2-source.uf2 input.bin output.uf2 [app-address] [family-id]\n", argv[0]);
        return 2;
    }
    if (argc >= 4 && !parse_u32(argv[3], &target_base)) {
        fprintf(stderr, "%s: invalid target address: %s\n", argv[0], argv[3]);
        return 2;
    }
    if (argc >= 5 && !parse_u32(argv[4], &family_id)) {
        fprintf(stderr, "%s: invalid family id: %s\n", argv[0], argv[4]);
        return 2;
    }
    return write_plain_uf2(argv[1], argv[2], target_base, family_id);
}
