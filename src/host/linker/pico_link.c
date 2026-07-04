#include "host_nolibc.h"

typedef unsigned long long u64;

#define MAX_OBJECTS 32
#define MAX_FILE_SIZE (512u * 1024u)
#define MAX_INPUT_SECTIONS 2048
#define MAX_GLOBAL_SYMBOLS 4096
#define MAX_OUTPUT_SIZE (1024u * 1024u)
#define MAX_PATH_LEN 256

#define EI_NIDENT 16
#define SHT_PROGBITS 1u
#define SHT_SYMTAB 2u
#define SHT_STRTAB 3u
#define SHT_NOBITS 8u
#define SHT_REL 9u
#define SHF_WRITE 1u
#define SHF_ALLOC 2u
#define SHF_EXECINSTR 4u
#define SHN_UNDEF 0u
#define SHN_ABS 0xfff1u
#define STB_LOCAL 0u
#define STB_GLOBAL 1u
#define STB_WEAK 2u
#define STT_SECTION 3u
#define R_ARM_ABS32 2u
#define R_ARM_THM_CALL 10u

#define FLASH_BASE 0x10000000u
#define APP_BASE 0x10032000u
#define RAM_BASE 0x20000000u
#define STACK_TOP 0x20042000u

typedef struct __attribute__((packed)) {
    u8 e_ident[EI_NIDENT];
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u32 e_entry;
    u32 e_phoff;
    u32 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
} Elf32Ehdr;

typedef struct __attribute__((packed)) {
    u32 sh_name;
    u32 sh_type;
    u32 sh_flags;
    u32 sh_addr;
    u32 sh_offset;
    u32 sh_size;
    u32 sh_link;
    u32 sh_info;
    u32 sh_addralign;
    u32 sh_entsize;
} Elf32Shdr;

typedef struct __attribute__((packed)) {
    u32 st_name;
    u32 st_value;
    u32 st_size;
    u8 st_info;
    u8 st_other;
    u16 st_shndx;
} Elf32Sym;

typedef struct __attribute__((packed)) {
    u32 r_offset;
    u32 r_info;
} Elf32Rel;

typedef struct {
    const char *path;
    u8 *data;
    usize size;
    const Elf32Ehdr *ehdr;
    const Elf32Shdr *sections;
    const char *section_names;
    const Elf32Sym *symbols;
    const char *symbol_names;
    u32 symbol_count;
} ObjectFile;

typedef enum {
    SEC_VECTORS,
    SEC_TEXT,
    SEC_RODATA,
    SEC_DATA,
    SEC_BSS
} SectionKind;

typedef struct {
    ObjectFile *object;
    u32 section_index;
    const Elf32Shdr *header;
    const char *name;
    SectionKind kind;
    u32 vma;
    u32 lma;
    u32 output_offset;
} InputSection;

typedef struct {
    const char *name;
    u32 value;
    u8 bind;
} GlobalSymbol;

static u8 g_file_storage[MAX_OBJECTS][MAX_FILE_SIZE];
static ObjectFile g_objects[MAX_OBJECTS];
static InputSection g_input_sections[MAX_INPUT_SECTIONS];
static GlobalSymbol g_globals[MAX_GLOBAL_SYMBOLS];
static u8 g_output[MAX_OUTPUT_SIZE];
static int g_object_count;
static int g_input_section_count;
static int g_global_count;
static u32 g_text_start;
static u32 g_text_end;
static u32 g_data_source;
static u32 g_data_start;
static u32 g_data_end;
static u32 g_bss_start;
static u32 g_bss_end;
static u32 g_output_size;

static u16 read16(const u8 *data) {
    return (u16)data[0] | ((u16)data[1] << 8);
}

static u32 read32(const u8 *data) {
    return (u32)data[0] | ((u32)data[1] << 8) | ((u32)data[2] << 16) | ((u32)data[3] << 24);
}

static void write16(u8 *data, u32 value) {
    data[0] = (u8)value;
    data[1] = (u8)(value >> 8);
}

static void write32(u8 *data, u32 value) {
    data[0] = (u8)value;
    data[1] = (u8)(value >> 8);
    data[2] = (u8)(value >> 16);
    data[3] = (u8)(value >> 24);
}

static u32 align_up(u32 value, u32 alignment) {
    if (alignment <= 1u) return value;
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static void fail_text(const char *message) {
    out("pico_link: ");
    out(message);
    out("\n");
    sys_exit(1);
}

static void fail_path(const char *message, const char *path) {
    out("pico_link: ");
    out(message);
    out(": ");
    out(path);
    out("\n");
    sys_exit(1);
}

static int name_is(const char *name, const char *want) {
    return str_eq(name, want);
}

static u8 elf_bind(const Elf32Sym *symbol) {
    return symbol->st_info >> 4;
}

static u32 rel_symbol_index(const Elf32Rel *relocation) {
    return relocation->r_info >> 8;
}

static u32 rel_type(const Elf32Rel *relocation) {
    return relocation->r_info & 0xffu;
}

static usize read_whole_file(const char *path, u8 *buffer, usize capacity) {
    long fd = sys_openat(AT_FDCWD, path, O_RDONLY, 0);
    usize total = 0;
    if (fd < 0) fail_path("open failed", path);
    while (total < capacity) {
        long got = sys_read((int)fd, buffer + total, capacity - total);
        if (got < 0) fail_path("read failed", path);
        if (got == 0) break;
        total += (usize)got;
    }
    (void)sys_close((int)fd);
    if (total == capacity) fail_path("file too large", path);
    return total;
}

static void write_whole_file(const char *path, const u8 *buffer, usize size) {
    long fd = sys_openat(AT_FDCWD, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    usize total = 0;
    if (fd < 0) fail_path("create failed", path);
    while (total < size) {
        long wrote = sys_write((int)fd, buffer + total, size - total);
        if (wrote <= 0) fail_path("write failed", path);
        total += (usize)wrote;
    }
    (void)sys_close((int)fd);
}

static const char *section_name(ObjectFile *object, u32 section_index) {
    const Elf32Shdr *section;
    if (section_index >= object->ehdr->e_shnum) return "";
    section = object->sections + section_index;
    return object->section_names + section->sh_name;
}

static InputSection *find_input_section(ObjectFile *object, u32 section_index) {
    int section_pos;
    for (section_pos = 0; section_pos < g_input_section_count; ++section_pos) {
        InputSection *input_section = g_input_sections + section_pos;
        if (input_section->object == object && input_section->section_index == section_index) return input_section;
    }
    return 0;
}

static void parse_object(ObjectFile *object, const char *path, u8 *storage) {
    u32 section_index;
    object->path = path;
    object->data = storage;
    object->size = read_whole_file(path, storage, MAX_FILE_SIZE);
    if (object->size < sizeof(Elf32Ehdr)) fail_path("not an ELF object", path);
    object->ehdr = (const Elf32Ehdr *)object->data;
    if (object->ehdr->e_ident[0] != 0x7fu || object->ehdr->e_ident[1] != 'E' || object->ehdr->e_ident[2] != 'L' || object->ehdr->e_ident[3] != 'F') fail_path("bad ELF magic", path);
    if (object->ehdr->e_ident[4] != 1u || object->ehdr->e_ident[5] != 1u) fail_path("expected ELF32 little-endian", path);
    if (object->ehdr->e_type != 1u || object->ehdr->e_machine != 40u) fail_path("expected ARM relocatable object", path);
    if (object->ehdr->e_shoff + (u32)object->ehdr->e_shnum * sizeof(Elf32Shdr) > object->size) fail_path("section table outside file", path);
    object->sections = (const Elf32Shdr *)(object->data + object->ehdr->e_shoff);
    if (object->ehdr->e_shstrndx >= object->ehdr->e_shnum) fail_path("missing section string table", path);
    object->section_names = (const char *)(object->data + object->sections[object->ehdr->e_shstrndx].sh_offset);
    object->symbols = 0;
    object->symbol_names = 0;
    object->symbol_count = 0;
    for (section_index = 0; section_index < object->ehdr->e_shnum; ++section_index) {
        const Elf32Shdr *section = object->sections + section_index;
        if (section->sh_offset + section->sh_size > object->size && section->sh_type != SHT_NOBITS) fail_path("section outside file", path);
        if (section->sh_type == SHT_SYMTAB) {
            if (section->sh_entsize != sizeof(Elf32Sym)) fail_path("unexpected symbol size", path);
            if (section->sh_link >= object->ehdr->e_shnum) fail_path("bad symbol string table", path);
            object->symbols = (const Elf32Sym *)(object->data + section->sh_offset);
            object->symbol_names = (const char *)(object->data + object->sections[section->sh_link].sh_offset);
            object->symbol_count = section->sh_size / sizeof(Elf32Sym);
        }
    }
    if (object->symbols == 0) fail_path("missing symbol table", path);
}

static SectionKind classify_section(const char *name, const Elf32Shdr *section) {
    if (name_is(name, ".vectors")) return SEC_VECTORS;
    if ((section->sh_flags & SHF_EXECINSTR) != 0u) return SEC_TEXT;
    if ((section->sh_flags & SHF_WRITE) != 0u && section->sh_type == SHT_NOBITS) return SEC_BSS;
    if ((section->sh_flags & SHF_WRITE) != 0u) return SEC_DATA;
    return SEC_RODATA;
}

static void collect_input_sections(ObjectFile *object) {
    u32 section_index;
    for (section_index = 1; section_index < object->ehdr->e_shnum; ++section_index) {
        const Elf32Shdr *section = object->sections + section_index;
        const char *name = section_name(object, section_index);
        InputSection *input_section;
        if ((section->sh_flags & SHF_ALLOC) == 0u) continue;
        if (section->sh_size == 0u) continue;
        if (section->sh_type != SHT_PROGBITS && section->sh_type != SHT_NOBITS) continue;
        if (g_input_section_count >= MAX_INPUT_SECTIONS) fail_text("too many input sections");
        input_section = g_input_sections + g_input_section_count++;
        input_section->object = object;
        input_section->section_index = section_index;
        input_section->header = section;
        input_section->name = name;
        input_section->kind = classify_section(name, section);
        input_section->vma = 0;
        input_section->lma = 0;
        input_section->output_offset = 0;
    }
}

static void copy_section_bytes(InputSection *input_section) {
    const Elf32Shdr *header = input_section->header;
    u32 output_end = input_section->output_offset + header->sh_size;
    if (header->sh_type == SHT_NOBITS) return;
    if (output_end > MAX_OUTPUT_SIZE) fail_text("output image too large");
    for (u32 byte_index = 0; byte_index < header->sh_size; ++byte_index) {
        g_output[input_section->output_offset + byte_index] = input_section->object->data[header->sh_offset + byte_index];
    }
}

static u32 layout_flash_sections(SectionKind kind, u32 cursor) {
    int section_pos;
    for (section_pos = 0; section_pos < g_input_section_count; ++section_pos) {
        InputSection *input_section = g_input_sections + section_pos;
        u32 alignment;
        if (input_section->kind != kind) continue;
        alignment = input_section->header->sh_addralign;
        if (alignment == 0u) alignment = 1u;
        cursor = align_up(cursor, alignment);
        input_section->vma = cursor;
        input_section->lma = cursor;
        input_section->output_offset = cursor - APP_BASE;
        copy_section_bytes(input_section);
        cursor += input_section->header->sh_size;
    }
    return cursor;
}

static void layout_data_sections(u32 *ram_cursor, u32 *flash_cursor) {
    int section_pos;
    *ram_cursor = align_up(*ram_cursor, 4u);
    *flash_cursor = align_up(*flash_cursor, 4u);
    g_data_start = *ram_cursor;
    g_data_source = *flash_cursor;
    for (section_pos = 0; section_pos < g_input_section_count; ++section_pos) {
        InputSection *input_section = g_input_sections + section_pos;
        u32 alignment;
        if (input_section->kind != SEC_DATA) continue;
        alignment = input_section->header->sh_addralign;
        if (alignment == 0u) alignment = 1u;
        *ram_cursor = align_up(*ram_cursor, alignment);
        *flash_cursor = align_up(*flash_cursor, alignment);
        input_section->vma = *ram_cursor;
        input_section->lma = *flash_cursor;
        input_section->output_offset = *flash_cursor - APP_BASE;
        copy_section_bytes(input_section);
        *ram_cursor += input_section->header->sh_size;
        *flash_cursor += input_section->header->sh_size;
    }
    g_data_end = *ram_cursor;
}

static void layout_bss_sections(u32 *ram_cursor) {
    int section_pos;
    *ram_cursor = align_up(*ram_cursor, 4u);
    g_bss_start = *ram_cursor;
    for (section_pos = 0; section_pos < g_input_section_count; ++section_pos) {
        InputSection *input_section = g_input_sections + section_pos;
        u32 alignment;
        if (input_section->kind != SEC_BSS) continue;
        alignment = input_section->header->sh_addralign;
        if (alignment == 0u) alignment = 1u;
        *ram_cursor = align_up(*ram_cursor, alignment);
        input_section->vma = *ram_cursor;
        input_section->lma = 0;
        input_section->output_offset = 0;
        *ram_cursor += input_section->header->sh_size;
    }
    g_bss_end = align_up(*ram_cursor, 4u);
}

static void layout_sections(void) {
    u32 flash_cursor = APP_BASE;
    u32 ram_cursor = RAM_BASE;
    g_text_start = flash_cursor;
    flash_cursor = layout_flash_sections(SEC_VECTORS, flash_cursor);
    flash_cursor = layout_flash_sections(SEC_TEXT, flash_cursor);
    flash_cursor = layout_flash_sections(SEC_RODATA, flash_cursor);
    flash_cursor = align_up(flash_cursor, 4u);
    g_text_end = flash_cursor;
    layout_data_sections(&ram_cursor, &flash_cursor);
    layout_bss_sections(&ram_cursor);
    g_output_size = flash_cursor - APP_BASE;
    if (g_output_size > MAX_OUTPUT_SIZE) fail_text("output image too large");
}

static int special_symbol_value(const char *name, u32 *out_value) {
    if (name_is(name, "__text_start")) { *out_value = g_text_start; return 1; }
    if (name_is(name, "__text_end")) { *out_value = g_text_end; return 1; }
    if (name_is(name, "__data_source")) { *out_value = g_data_source; return 1; }
    if (name_is(name, "__data_start")) { *out_value = g_data_start; return 1; }
    if (name_is(name, "__data_end")) { *out_value = g_data_end; return 1; }
    if (name_is(name, "__bss_start")) { *out_value = g_bss_start; return 1; }
    if (name_is(name, "__bss_end")) { *out_value = g_bss_end; return 1; }
    if (name_is(name, "__StackTop") || name_is(name, "__stack")) { *out_value = STACK_TOP; return 1; }
    return 0;
}

static int symbol_value(ObjectFile *object, u32 symbol_index, u32 *out_value) {
    const Elf32Sym *symbol;
    InputSection *input_section;
    const char *name;
    if (symbol_index >= object->symbol_count) fail_path("bad relocation symbol", object->path);
    symbol = object->symbols + symbol_index;
    if (symbol->st_shndx == SHN_ABS) {
        *out_value = symbol->st_value;
        return 1;
    }
    if (symbol->st_shndx != SHN_UNDEF) {
        input_section = find_input_section(object, symbol->st_shndx);
        if (input_section == 0) fail_path("symbol in discarded section", object->path);
        *out_value = input_section->vma + symbol->st_value;
        return 1;
    }
    name = object->symbol_names + symbol->st_name;
    if (special_symbol_value(name, out_value)) return 1;
    for (int global_index = 0; global_index < g_global_count; ++global_index) {
        if (str_eq(name, g_globals[global_index].name)) {
            *out_value = g_globals[global_index].value;
            return 1;
        }
    }
    if (elf_bind(symbol) == STB_WEAK) {
        *out_value = 0;
        return 1;
    }
    out("pico_link: unresolved symbol: ");
    out(name);
    out(" in ");
    out(object->path);
    out("\n");
    return 0;
}

static void add_global_symbol(const char *name, u32 value, u8 bind) {
    int global_index;
    if (name == 0 || name[0] == 0) return;
    for (global_index = 0; global_index < g_global_count; ++global_index) {
        if (str_eq(name, g_globals[global_index].name)) {
            if (g_globals[global_index].bind == STB_WEAK && bind == STB_GLOBAL) {
                g_globals[global_index].value = value;
                g_globals[global_index].bind = bind;
                return;
            }
            if (g_globals[global_index].bind == STB_GLOBAL && bind == STB_GLOBAL) {
                out("pico_link: duplicate symbol: ");
                out(name);
                out("\n");
                sys_exit(1);
            }
            return;
        }
    }
    if (g_global_count >= MAX_GLOBAL_SYMBOLS) fail_text("too many global symbols");
    g_globals[g_global_count].name = name;
    g_globals[g_global_count].value = value;
    g_globals[g_global_count].bind = bind;
    g_global_count += 1;
}

static void collect_global_symbols(void) {
    int object_index;
    for (object_index = 0; object_index < g_object_count; ++object_index) {
        ObjectFile *object = g_objects + object_index;
        u32 symbol_index;
        for (symbol_index = 0; symbol_index < object->symbol_count; ++symbol_index) {
            const Elf32Sym *symbol = object->symbols + symbol_index;
            u8 bind = elf_bind(symbol);
            u32 value;
            if (bind != STB_GLOBAL && bind != STB_WEAK) continue;
            if (symbol->st_shndx == SHN_UNDEF) continue;
            if (!symbol_value(object, symbol_index, &value)) sys_exit(1);
            add_global_symbol(object->symbol_names + symbol->st_name, value, bind);
        }
    }
}

static s32 sign_extend(u32 value, int bits) {
    u32 mask = 1u << (bits - 1);
    return (s32)((value ^ mask) - mask);
}

static s32 decode_thumb_call_addend(u16 upper, u16 lower) {
    u32 sign = (upper >> 10) & 1u;
    u32 j1 = (lower >> 13) & 1u;
    u32 j2 = (lower >> 11) & 1u;
    u32 i1 = (~(j1 ^ sign)) & 1u;
    u32 i2 = (~(j2 ^ sign)) & 1u;
    u32 imm10 = upper & 0x03ffu;
    u32 imm11 = lower & 0x07ffu;
    u32 encoded = (sign << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1);
    return sign_extend(encoded, 25);
}

static void encode_thumb_call(u8 *location, s32 offset) {
    u16 old_upper = read16(location);
    u16 old_lower = read16(location + 2);
    u32 encoded = (u32)offset;
    u32 sign = (encoded >> 24) & 1u;
    u32 i1 = (encoded >> 23) & 1u;
    u32 i2 = (encoded >> 22) & 1u;
    u32 imm10 = (encoded >> 12) & 0x03ffu;
    u32 imm11 = (encoded >> 1) & 0x07ffu;
    u32 j1 = (~(i1 ^ sign)) & 1u;
    u32 j2 = (~(i2 ^ sign)) & 1u;
    u16 new_upper = (u16)((old_upper & 0xf800u) | (sign << 10) | imm10);
    u16 new_lower = (u16)((old_lower & 0xd000u) | (j1 << 13) | (j2 << 11) | imm11);
    write16(location, new_upper);
    write16(location + 2, new_lower);
}

static void apply_abs32(ObjectFile *object, InputSection *target_section, const Elf32Rel *relocation) {
    u8 *location = g_output + target_section->output_offset + relocation->r_offset;
    u32 symbol;
    u32 addend = read32(location);
    if (!symbol_value(object, rel_symbol_index(relocation), &symbol)) sys_exit(1);
    write32(location, symbol + addend);
}

static void apply_thumb_call(ObjectFile *object, InputSection *target_section, const Elf32Rel *relocation) {
    u8 *location = g_output + target_section->output_offset + relocation->r_offset;
    u16 upper = read16(location);
    u16 lower = read16(location + 2);
    s32 addend = decode_thumb_call_addend(upper, lower);
    u32 symbol;
    u32 place = target_section->vma + relocation->r_offset;
    s32 offset;
    if (!symbol_value(object, rel_symbol_index(relocation), &symbol)) sys_exit(1);
    offset = (s32)((symbol + (u32)addend - place) & ~1u);
    if (offset < -16777216 || offset > 16777214) fail_path("Thumb call target out of range", object->path);
    encode_thumb_call(location, offset);
}

static void apply_relocations(void) {
    int object_index;
    for (object_index = 0; object_index < g_object_count; ++object_index) {
        ObjectFile *object = g_objects + object_index;
        u32 section_index;
        for (section_index = 0; section_index < object->ehdr->e_shnum; ++section_index) {
            const Elf32Shdr *rel_section = object->sections + section_index;
            InputSection *target_section;
            u32 relocation_count;
            if (rel_section->sh_type != SHT_REL) continue;
            if (rel_section->sh_entsize != sizeof(Elf32Rel)) fail_path("unexpected relocation size", object->path);
            target_section = find_input_section(object, rel_section->sh_info);
            if (target_section == 0) continue;
            relocation_count = rel_section->sh_size / sizeof(Elf32Rel);
            for (u32 relocation_index = 0; relocation_index < relocation_count; ++relocation_index) {
                const Elf32Rel *relocation = (const Elf32Rel *)(object->data + rel_section->sh_offset + relocation_index * sizeof(Elf32Rel));
                u32 type = rel_type(relocation);
                if (type == R_ARM_ABS32) apply_abs32(object, target_section, relocation);
                else if (type == R_ARM_THM_CALL) apply_thumb_call(object, target_section, relocation);
                else {
                    out("pico_link: unsupported relocation ");
                    out_hex(type);
                    out(" in ");
                    out(object->path);
                    out("\n");
                    sys_exit(1);
                }
            }
        }
    }
}

static void usage(void) {
    out("usage: pico_link -o output.bin input.o...\n");
}

__attribute__((used)) int linker_main(int argc, char **argv) {
    const char *output_path = 0;
    int arg_index = 1;
    while (arg_index < argc) {
        if (str_eq(argv[arg_index], "-o")) {
            arg_index += 1;
            if (arg_index >= argc) { usage(); return 1; }
            output_path = argv[arg_index++];
        } else {
            break;
        }
    }
    if (output_path == 0 || arg_index >= argc) {
        usage();
        return 1;
    }
    while (arg_index < argc) {
        if (g_object_count >= MAX_OBJECTS) fail_text("too many input files");
        parse_object(g_objects + g_object_count, argv[arg_index], g_file_storage[g_object_count]);
        collect_input_sections(g_objects + g_object_count);
        g_object_count += 1;
        arg_index += 1;
    }
    layout_sections();
    collect_global_symbols();
    apply_relocations();
    write_whole_file(output_path, g_output, g_output_size);
    out("pico_link: wrote ");
    out(output_path);
    out(" size=");
    out_hex(g_output_size);
    out("\n");
    return 0;
}

__attribute__((naked, used)) void _start(void) {
    __asm__ volatile(
        "mov (%%rsp), %%rdi\n"
        "lea 8(%%rsp), %%rsi\n"
        "and $-16, %%rsp\n"
        "call linker_main\n"
        "mov %%eax, %%edi\n"
        "call sys_exit\n"
        : : : "memory");
}