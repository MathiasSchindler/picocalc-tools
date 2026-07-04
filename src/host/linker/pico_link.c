#include "host_nolibc.h"

typedef unsigned long long u64;

#define MAX_OBJECTS 32
#define MAX_ARCHIVES 8
#define MAX_FILE_SIZE (512u * 1024u)
#define MAX_ARCHIVE_SIZE (2u * 1024u * 1024u)
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
#define R_ARM_REL32 3u
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
    int live;
    int live_order;
    u32 vma;
    u32 lma;
    u32 output_offset;
} InputSection;

typedef struct {
    const char *name;
    u32 value;
    u8 bind;
    ObjectFile *object;
    u32 symbol_index;
} GlobalSymbol;

static u8 g_file_storage[MAX_OBJECTS][MAX_FILE_SIZE];
static u8 g_archive_storage[MAX_ARCHIVE_SIZE];
static char g_object_path_storage[MAX_OBJECTS][MAX_PATH_LEN];
static ObjectFile g_objects[MAX_OBJECTS];
static const char *g_archives[MAX_ARCHIVES];
static InputSection g_input_sections[MAX_INPUT_SECTIONS];
static GlobalSymbol g_globals[MAX_GLOBAL_SYMBOLS];
static u8 g_output[MAX_OUTPUT_SIZE];
static int g_object_count;
static int g_archive_count;
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
static int g_live_section_count;
static int g_discarded_section_count;
static int g_relocation_count;
static int g_relocation_abs32_count;
static int g_relocation_rel32_count;
static int g_relocation_thm_call_count;
static int g_archive_member_count;
static int g_gc_sections = 1;
static int g_print_stats;
static int g_order_by_reach;
static const char *g_map_path;

static int special_symbol_value(const char *name, u32 *out_value);
static int symbol_is_unresolved(const char *name);
static void add_global_symbol(const char *name, u32 value, u8 bind, ObjectFile *object, u32 symbol_index);

static void copy_bytes(u8 *dst, const u8 *src, usize size) {
    for (usize index = 0; index < size; ++index) dst[index] = src[index];
}

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

static int fixed_name_is(const char *name, const char *want, usize count) {
    usize index = 0;
    while (index < count && want[index] != 0) {
        if (name[index] != want[index]) return 0;
        index += 1u;
    }
    if (want[index] != 0) return 0;
    while (index < count) {
        if (name[index] != ' ') return 0;
        index += 1u;
    }
    return 1;
}

static u32 parse_archive_decimal(const char *text, usize count) {
    u32 value = 0;
    usize index = 0;
    int any = 0;
    while (index < count && text[index] == ' ') index += 1u;
    while (index < count && text[index] >= '0' && text[index] <= '9') {
        value = value * 10u + (u32)(text[index] - '0');
        any = 1;
        index += 1u;
    }
    while (index < count && text[index] == ' ') index += 1u;
    if (!any || index != count) fail_text("bad archive decimal field");
    return value;
}

static usize archive_member_name_len(const char *name) {
    usize count = 16;
    while (count > 0u && name[count - 1u] == ' ') count -= 1u;
    if (count > 0u && name[count - 1u] == '/') count -= 1u;
    return count;
}

static void make_archive_member_path(const char *archive_path, const char *member_name, char *out_path) {
    usize out_pos = 0;
    usize name_len = archive_member_name_len(member_name);
    for (usize index = 0; archive_path[index] != 0 && out_pos + 1u < MAX_PATH_LEN; ++index) out_path[out_pos++] = archive_path[index];
    if (out_pos + 1u < MAX_PATH_LEN) out_path[out_pos++] = '(';
    for (usize index = 0; index < name_len && out_pos + 1u < MAX_PATH_LEN; ++index) out_path[out_pos++] = member_name[index];
    if (out_pos + 1u < MAX_PATH_LEN) out_path[out_pos++] = ')';
    out_path[out_pos] = 0;
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

static void parse_object_data(ObjectFile *object, const char *path, u8 *storage, usize size) {
    u32 section_index;
    object->path = path;
    object->data = storage;
    object->size = size;
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

static void parse_object(ObjectFile *object, const char *path, u8 *storage) {
    usize size = read_whole_file(path, storage, MAX_FILE_SIZE);
    parse_object_data(object, path, storage, size);
}

static int object_buffer_defines_needed(const u8 *data, usize size) {
    const Elf32Ehdr *ehdr;
    const Elf32Shdr *sections;
    const Elf32Sym *symbols = 0;
    const char *symbol_names = 0;
    u32 symbol_count = 0;
    if (size < sizeof(Elf32Ehdr)) return 0;
    ehdr = (const Elf32Ehdr *)data;
    if (ehdr->e_ident[0] != 0x7fu || ehdr->e_ident[1] != 'E' || ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') return 0;
    if (ehdr->e_ident[4] != 1u || ehdr->e_ident[5] != 1u) return 0;
    if (ehdr->e_type != 1u || ehdr->e_machine != 40u) return 0;
    if (ehdr->e_shoff + (u32)ehdr->e_shnum * sizeof(Elf32Shdr) > size) return 0;
    sections = (const Elf32Shdr *)(data + ehdr->e_shoff);
    for (u32 section_index = 0; section_index < ehdr->e_shnum; ++section_index) {
        const Elf32Shdr *section = sections + section_index;
        if (section->sh_offset + section->sh_size > size && section->sh_type != SHT_NOBITS) return 0;
        if (section->sh_type == SHT_SYMTAB) {
            if (section->sh_entsize != sizeof(Elf32Sym)) return 0;
            if (section->sh_link >= ehdr->e_shnum) return 0;
            symbols = (const Elf32Sym *)(data + section->sh_offset);
            symbol_names = (const char *)(data + sections[section->sh_link].sh_offset);
            symbol_count = section->sh_size / sizeof(Elf32Sym);
        }
    }
    if (symbols == 0) return 0;
    for (u32 symbol_index = 0; symbol_index < symbol_count; ++symbol_index) {
        const Elf32Sym *symbol = symbols + symbol_index;
        u8 bind = elf_bind(symbol);
        if (bind != STB_GLOBAL && bind != STB_WEAK) continue;
        if (symbol->st_shndx == SHN_UNDEF) continue;
        if (symbol_is_unresolved(symbol_names + symbol->st_name)) return 1;
    }
    return 0;
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
        input_section->live = 0;
        input_section->live_order = -1;
        input_section->vma = 0;
        input_section->lma = 0;
        input_section->output_offset = 0;
    }
}

static void mark_section_live(InputSection *input_section) {
    if (input_section == 0 || input_section->live) return;
    input_section->live = 1;
    input_section->live_order = g_live_section_count++;
}

static InputSection *symbol_target_section(ObjectFile *object, u32 symbol_index) {
    const Elf32Sym *symbol;
    u32 value;
    if (symbol_index >= object->symbol_count) fail_path("bad relocation symbol", object->path);
    symbol = object->symbols + symbol_index;
    if (symbol->st_shndx != SHN_UNDEF && symbol->st_shndx != SHN_ABS) return find_input_section(object, symbol->st_shndx);
    if (symbol->st_shndx == SHN_ABS) return 0;
    if (special_symbol_value(object->symbol_names + symbol->st_name, &value)) return 0;
    for (int global_index = 0; global_index < g_global_count; ++global_index) {
        if (str_eq(object->symbol_names + symbol->st_name, g_globals[global_index].name)) {
            GlobalSymbol *global = g_globals + global_index;
            if (global->object == 0) return 0;
            return symbol_target_section(global->object, global->symbol_index);
        }
    }
    if (elf_bind(symbol) == STB_WEAK) return 0;
    out("pico_link: unresolved symbol during gc: ");
    out(object->symbol_names + symbol->st_name);
    out(" in ");
    out(object->path);
    out("\n");
    sys_exit(1);
}

static void collect_object_global_symbol_names(ObjectFile *object) {
    u32 symbol_index;
    for (symbol_index = 0; symbol_index < object->symbol_count; ++symbol_index) {
        const Elf32Sym *symbol = object->symbols + symbol_index;
        u8 bind = elf_bind(symbol);
        if (bind != STB_GLOBAL && bind != STB_WEAK) continue;
        if (symbol->st_shndx == SHN_UNDEF) continue;
        add_global_symbol(object->symbol_names + symbol->st_name, 0, bind, object, symbol_index);
    }
}

static void collect_global_symbol_names(void) {
    int object_index;
    for (object_index = 0; object_index < g_object_count; ++object_index) {
        collect_object_global_symbol_names(g_objects + object_index);
    }
}

static int symbol_is_unresolved(const char *name) {
    u32 value;
    if (special_symbol_value(name, &value)) return 0;
    for (int global_index = 0; global_index < g_global_count; ++global_index) {
        if (str_eq(name, g_globals[global_index].name)) return 0;
    }
    for (int object_index = 0; object_index < g_object_count; ++object_index) {
        ObjectFile *object = g_objects + object_index;
        for (u32 symbol_index = 0; symbol_index < object->symbol_count; ++symbol_index) {
            const Elf32Sym *symbol = object->symbols + symbol_index;
            if (symbol->st_shndx != SHN_UNDEF) continue;
            if (elf_bind(symbol) != STB_GLOBAL) continue;
            if (str_eq(name, object->symbol_names + symbol->st_name)) return 1;
        }
    }
    return 0;
}

static void include_archive_member(const char *archive_path, const char *member_name, const u8 *data, usize size) {
    ObjectFile *object;
    if (g_object_count >= MAX_OBJECTS) fail_text("too many input files");
    if (size > MAX_FILE_SIZE) fail_path("archive member too large", archive_path);
    make_archive_member_path(archive_path, member_name, g_object_path_storage[g_object_count]);
    copy_bytes(g_file_storage[g_object_count], data, size);
    object = g_objects + g_object_count;
    parse_object_data(object, g_object_path_storage[g_object_count], g_file_storage[g_object_count], size);
    collect_input_sections(object);
    g_object_count += 1;
    g_archive_member_count += 1;
    collect_object_global_symbol_names(object);
}

static int scan_archive_once(const char *path) {
    usize size = read_whole_file(path, g_archive_storage, MAX_ARCHIVE_SIZE);
    usize offset = 8u;
    int included = 0;
    if (size < 8u || !fixed_name_is((const char *)g_archive_storage, "!<arch>\n", 8u)) fail_path("not an archive", path);
    while (offset + 60u <= size) {
        const char *header = (const char *)(g_archive_storage + offset);
        u32 member_size;
        const u8 *member_data;
        if (header[58] != '`' || header[59] != '\n') fail_path("bad archive header", path);
        member_size = parse_archive_decimal(header + 48, 10u);
        member_data = g_archive_storage + offset + 60u;
        if (offset + 60u + member_size > size) fail_path("archive member outside file", path);
        if (!fixed_name_is(header, "/", 16u) && !fixed_name_is(header, "//", 16u) && header[0] != '/') {
            if (object_buffer_defines_needed(member_data, member_size)) {
                include_archive_member(path, header, member_data, member_size);
                included = 1;
            }
        }
        offset += 60u + member_size;
        if ((offset & 1u) != 0u) offset += 1u;
    }
    return included;
}

static void resolve_archives(void) {
    int changed;
    do {
        changed = 0;
        for (int archive_index = 0; archive_index < g_archive_count; ++archive_index) {
            if (scan_archive_once(g_archives[archive_index])) changed = 1;
        }
    } while (changed);
}

static void mark_reachable_sections(void) {
    int changed = 1;
    int section_pos;
    if (!g_gc_sections) {
        for (section_pos = 0; section_pos < g_input_section_count; ++section_pos) mark_section_live(g_input_sections + section_pos);
        return;
    }
    for (section_pos = 0; section_pos < g_input_section_count; ++section_pos) {
        if (g_input_sections[section_pos].kind == SEC_VECTORS) mark_section_live(g_input_sections + section_pos);
    }
    while (changed) {
        changed = 0;
        for (section_pos = 0; section_pos < g_input_section_count; ++section_pos) {
            InputSection *input_section = g_input_sections + section_pos;
            ObjectFile *object;
            u32 rel_index;
            if (!input_section->live) continue;
            object = input_section->object;
            for (rel_index = 0; rel_index < object->ehdr->e_shnum; ++rel_index) {
                const Elf32Shdr *rel_section = object->sections + rel_index;
                u32 relocation_count;
                if (rel_section->sh_type != SHT_REL || rel_section->sh_info != input_section->section_index) continue;
                if (rel_section->sh_entsize != sizeof(Elf32Rel)) fail_path("unexpected relocation size", object->path);
                relocation_count = rel_section->sh_size / sizeof(Elf32Rel);
                for (u32 relocation_index = 0; relocation_index < relocation_count; ++relocation_index) {
                    const Elf32Rel *relocation = (const Elf32Rel *)(object->data + rel_section->sh_offset + relocation_index * sizeof(Elf32Rel));
                    InputSection *target = symbol_target_section(object, rel_symbol_index(relocation));
                    if (target != 0 && !target->live) {
                        mark_section_live(target);
                        changed = 1;
                    }
                }
            }
        }
    }
    for (section_pos = 0; section_pos < g_input_section_count; ++section_pos) if (!g_input_sections[section_pos].live) g_discarded_section_count += 1;
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
    int placed;
    do {
        InputSection *input_section = 0;
        u32 alignment;
        placed = 0;
        for (int section_pos = 0; section_pos < g_input_section_count; ++section_pos) {
            InputSection *candidate = g_input_sections + section_pos;
            if (candidate->kind != kind || !candidate->live || candidate->vma != 0u) continue;
            if (input_section == 0 || (g_order_by_reach && candidate->live_order < input_section->live_order)) input_section = candidate;
        }
        if (input_section == 0) break;
        alignment = input_section->header->sh_addralign;
        if (alignment == 0u) alignment = 1u;
        cursor = align_up(cursor, alignment);
        input_section->vma = cursor;
        input_section->lma = cursor;
        input_section->output_offset = cursor - APP_BASE;
        copy_section_bytes(input_section);
        cursor += input_section->header->sh_size;
        placed = 1;
    } while (placed);
    return cursor;
}

static void layout_data_sections(u32 *ram_cursor, u32 *flash_cursor) {
    *ram_cursor = align_up(*ram_cursor, 4u);
    *flash_cursor = align_up(*flash_cursor, 4u);
    g_data_start = *ram_cursor;
    g_data_source = *flash_cursor;
    while (1) {
        InputSection *input_section = 0;
        u32 alignment;
        for (int section_pos = 0; section_pos < g_input_section_count; ++section_pos) {
            InputSection *candidate = g_input_sections + section_pos;
            if (candidate->kind != SEC_DATA || !candidate->live || candidate->vma != 0u) continue;
            if (input_section == 0 || (g_order_by_reach && candidate->live_order < input_section->live_order)) input_section = candidate;
        }
        if (input_section == 0) break;
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
    *ram_cursor = align_up(*ram_cursor, 4u);
    g_bss_start = *ram_cursor;
    while (1) {
        InputSection *input_section = 0;
        u32 alignment;
        for (int section_pos = 0; section_pos < g_input_section_count; ++section_pos) {
            InputSection *candidate = g_input_sections + section_pos;
            if (candidate->kind != SEC_BSS || !candidate->live || candidate->vma != 0u) continue;
            if (input_section == 0 || (g_order_by_reach && candidate->live_order < input_section->live_order)) input_section = candidate;
        }
        if (input_section == 0) break;
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

static void add_global_symbol(const char *name, u32 value, u8 bind, ObjectFile *object, u32 symbol_index) {
    int global_index;
    if (name == 0 || name[0] == 0) return;
    for (global_index = 0; global_index < g_global_count; ++global_index) {
        if (str_eq(name, g_globals[global_index].name)) {
            if (g_globals[global_index].object == object && g_globals[global_index].symbol_index == symbol_index) {
                g_globals[global_index].value = value;
                g_globals[global_index].bind = bind;
                return;
            }
            if (g_globals[global_index].bind == STB_WEAK && bind == STB_GLOBAL) {
                g_globals[global_index].value = value;
                g_globals[global_index].bind = bind;
                g_globals[global_index].object = object;
                g_globals[global_index].symbol_index = symbol_index;
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
    g_globals[g_global_count].object = object;
    g_globals[g_global_count].symbol_index = symbol_index;
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
            InputSection *input_section;
            u32 value;
            if (bind != STB_GLOBAL && bind != STB_WEAK) continue;
            if (symbol->st_shndx == SHN_UNDEF) continue;
            if (symbol->st_shndx != SHN_ABS) {
                input_section = find_input_section(object, symbol->st_shndx);
                if (input_section == 0 || !input_section->live) continue;
            }
            if (!symbol_value(object, symbol_index, &value)) sys_exit(1);
            add_global_symbol(object->symbol_names + symbol->st_name, value, bind, object, symbol_index);
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

static void apply_rel32(ObjectFile *object, InputSection *target_section, const Elf32Rel *relocation) {
    u8 *location = g_output + target_section->output_offset + relocation->r_offset;
    u32 symbol;
    u32 addend = read32(location);
    u32 place = target_section->vma + relocation->r_offset;
    if (!symbol_value(object, rel_symbol_index(relocation), &symbol)) sys_exit(1);
    write32(location, symbol + addend - place);
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
            if (!target_section->live) continue;
            relocation_count = rel_section->sh_size / sizeof(Elf32Rel);
            for (u32 relocation_index = 0; relocation_index < relocation_count; ++relocation_index) {
                const Elf32Rel *relocation = (const Elf32Rel *)(object->data + rel_section->sh_offset + relocation_index * sizeof(Elf32Rel));
                u32 type = rel_type(relocation);
                g_relocation_count += 1;
                if (type == R_ARM_ABS32) { g_relocation_abs32_count += 1; apply_abs32(object, target_section, relocation); }
                else if (type == R_ARM_REL32) { g_relocation_rel32_count += 1; apply_rel32(object, target_section, relocation); }
                else if (type == R_ARM_THM_CALL) { g_relocation_thm_call_count += 1; apply_thumb_call(object, target_section, relocation); }
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

static const char *section_kind_name(SectionKind kind) {
    if (kind == SEC_VECTORS) return "vectors";
    if (kind == SEC_TEXT) return "text";
    if (kind == SEC_RODATA) return "rodata";
    if (kind == SEC_DATA) return "data";
    if (kind == SEC_BSS) return "bss";
    return "unknown";
}

static void out_uint(u32 value) {
    char buf[10];
    int count = 0;
    if (value == 0u) {
        out("0");
        return;
    }
    while (value != 0u && count < (int)sizeof(buf)) {
        buf[count++] = (char)('0' + value % 10u);
        value /= 10u;
    }
    while (count-- > 0) (void)sys_write(1, buf + count, 1);
}

static void write_map_line(int fd, const char *a, const char *b, const char *c) {
    (void)sys_write(fd, a, str_len(a));
    if (b != 0) (void)sys_write(fd, b, str_len(b));
    if (c != 0) (void)sys_write(fd, c, str_len(c));
    (void)sys_write(fd, "\n", 1);
}

static void write_map_hex(int fd, u32 value) {
    char buf[10];
    static const char hex[] = "0123456789abcdef";
    int i;
    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < 8; ++i) buf[2 + i] = hex[(value >> (28 - i * 4)) & 0xfu];
    (void)sys_write(fd, buf, sizeof(buf));
}

static void write_map_dec(int fd, u32 value) {
    char buf[10];
    int count = 0;
    if (value == 0u) {
        (void)sys_write(fd, "0", 1);
        return;
    }
    while (value != 0u && count < (int)sizeof(buf)) {
        buf[count++] = (char)('0' + value % 10u);
        value /= 10u;
    }
    while (count-- > 0) (void)sys_write(fd, buf + count, 1);
}

static void write_map_section(int fd, InputSection *input_section) {
    write_map_hex(fd, input_section->live ? input_section->vma : 0u);
    (void)sys_write(fd, " ", 1);
    write_map_hex(fd, input_section->live ? input_section->lma : 0u);
    (void)sys_write(fd, " ", 1);
    write_map_dec(fd, input_section->header->sh_size);
    (void)sys_write(fd, " ", 1);
    (void)sys_write(fd, input_section->live ? "keep " : "drop ", 5);
    write_map_line(fd, section_kind_name(input_section->kind), " ", input_section->name);
}

static void write_map(const char *path) {
    long fd = sys_openat(AT_FDCWD, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int section_pos;
    if (fd < 0) fail_path("create map failed", path);
    write_map_line((int)fd, "PICOLINK MAP", 0, 0);
    write_map_line((int)fd, "", 0, 0);
    write_map_line((int)fd, "SECTIONS", 0, 0);
    for (section_pos = 0; section_pos < g_input_section_count; ++section_pos) {
        InputSection *next_section = 0;
        for (int candidate_pos = 0; candidate_pos < g_input_section_count; ++candidate_pos) {
            InputSection *candidate = g_input_sections + candidate_pos;
            if (!candidate->live) continue;
            if (candidate->vma == 0u) continue;
            if (candidate->live_order < 0) continue;
            if (next_section == 0 || candidate->vma < next_section->vma) next_section = candidate;
        }
        if (next_section == 0) break;
        write_map_section((int)fd, next_section);
        next_section->live_order = -1;
    }
    for (section_pos = 0; section_pos < g_input_section_count; ++section_pos) {
        InputSection *input_section = g_input_sections + section_pos;
        if (!input_section->live) write_map_section((int)fd, input_section);
    }
    write_map_line((int)fd, "", 0, 0);
    write_map_line((int)fd, "SYMBOLS", 0, 0);
    for (int global_index = 0; global_index < g_global_count; ++global_index) {
        write_map_hex((int)fd, g_globals[global_index].value);
        (void)sys_write((int)fd, " ", 1);
        write_map_line((int)fd, g_globals[global_index].name, 0, 0);
    }
    (void)sys_close((int)fd);
}

static void print_stats(void) {
    out("pico_link: sections kept="); out_uint((u32)g_live_section_count);
    out(" discarded="); out_uint((u32)g_discarded_section_count);
    out(" archive_members="); out_uint((u32)g_archive_member_count);
    out(" relocations="); out_uint((u32)g_relocation_count);
    out(" abs32="); out_uint((u32)g_relocation_abs32_count);
    out(" rel32="); out_uint((u32)g_relocation_rel32_count);
    out(" thm_call="); out_uint((u32)g_relocation_thm_call_count);
    out(" image="); out_hex(g_output_size);
    out(" bss="); out_hex(g_bss_end - g_bss_start);
    out("\n");
}

static void usage(void) {
    out("usage: pico_link [-v] [--stats] [--map=path] [--no-gc-sections] [--order=reach] -o output.bin input.o...\n");
}

__attribute__((used)) int linker_main(int argc, char **argv) {
    const char *output_path = 0;
    int arg_index = 1;
    while (arg_index < argc) {
        if (str_eq(argv[arg_index], "-o")) {
            arg_index += 1;
            if (arg_index >= argc) { usage(); return 1; }
            output_path = argv[arg_index++];
        } else if (str_eq(argv[arg_index], "--stats") || str_eq(argv[arg_index], "-v")) {
            g_print_stats = 1;
            arg_index += 1;
        } else if (str_starts(argv[arg_index], "--map=")) {
            g_map_path = argv[arg_index] + 6;
            arg_index += 1;
        } else if (str_eq(argv[arg_index], "--map")) {
            arg_index += 1;
            if (arg_index >= argc) { usage(); return 1; }
            g_map_path = argv[arg_index++];
        } else if (str_eq(argv[arg_index], "--no-gc-sections")) {
            g_gc_sections = 0;
            arg_index += 1;
        } else if (str_eq(argv[arg_index], "--order=reach")) {
            g_order_by_reach = 1;
            arg_index += 1;
        } else if (str_eq(argv[arg_index], "--order=none")) {
            g_order_by_reach = 0;
            arg_index += 1;
        } else {
            break;
        }
    }
    if (output_path == 0 || arg_index >= argc) {
        usage();
        return 1;
    }
    while (arg_index < argc) {
        if (str_ends(argv[arg_index], ".a")) {
            if (g_archive_count >= MAX_ARCHIVES) fail_text("too many archive inputs");
            g_archives[g_archive_count++] = argv[arg_index];
        } else {
            if (g_object_count >= MAX_OBJECTS) fail_text("too many input files");
            parse_object(g_objects + g_object_count, argv[arg_index], g_file_storage[g_object_count]);
            collect_input_sections(g_objects + g_object_count);
            g_object_count += 1;
        }
        arg_index += 1;
    }
    collect_global_symbol_names();
    resolve_archives();
    mark_reachable_sections();
    layout_sections();
    collect_global_symbols();
    apply_relocations();
    write_whole_file(output_path, g_output, g_output_size);
    if (g_map_path != 0) write_map(g_map_path);
    out("pico_link: wrote ");
    out(output_path);
    out(" size=");
    out_hex(g_output_size);
    out("\n");
    if (g_print_stats) print_stats();
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