#include "emu_bootrom.h"
#include "emu_trace.h"
#include "emu_util.h"

#define FLASH_BASE 0x10000000u
#define BOOT2_BASE 0x10000000u
#define BOOT2_SIZE 0x00000100u

#define BOOTROM_FUNC_TABLE     0x00000200u
#define BOOTROM_DATA_TABLE     0x00000300u
#define BOOTROM_SD_TABLE       0x00000402u
#define BOOTROM_SF_TABLE       0x00000482u
#define BOOTROM_FN_L3          0x00001100u
#define BOOTROM_FN_P3          0x00001110u
#define BOOTROM_FN_R3          0x00001120u
#define BOOTROM_FN_T3          0x00001130u
#define BOOTROM_FN_MS          0x00001140u
#define BOOTROM_FN_MC          0x00001150u
#define BOOTROM_FN_S4          0x00001160u
#define BOOTROM_FN_C4          0x00001170u
#define ROM_CODE_SD            0x00004453u
#define ROM_CODE_SF            0x00004653u
#define ROM_CODE_L3            0x0000334cu
#define ROM_CODE_P3            0x00003350u
#define ROM_CODE_R3            0x00003352u
#define ROM_CODE_T3            0x00003354u
#define ROM_CODE_MS            0x0000534du
#define ROM_CODE_MC            0x0000434du
#define ROM_CODE_S4            0x00003453u
#define ROM_CODE_C4            0x00003443u
#define BOOTROM_FN_IF          0x00001180u
#define BOOTROM_FN_EX          0x00001190u
#define BOOTROM_FN_FC          0x000011a0u
#define BOOTROM_FN_RE          0x000011b0u
#define BOOTROM_FN_RP          0x000011c0u
#define BOOTROM_FN_FADD        0x00001200u
#define BOOTROM_FN_FSUB        0x00001210u
#define BOOTROM_FN_FMUL        0x00001220u
#define BOOTROM_FN_FDIV        0x00001230u
#define BOOTROM_FN_F2I         0x00001240u
#define ROM_CODE_IF            0x00004649u
#define ROM_CODE_EX            0x00005845u
#define ROM_CODE_FC            0x00004346u
#define ROM_CODE_RE            0x00004552u
#define ROM_CODE_RP            0x00005052u

int emu_boot2_stub_read8(u32 addr, u8 *out_value) {
    u32 shift;
    if (addr < BOOT2_BASE || addr >= BOOT2_BASE + BOOT2_SIZE) return 0;
    shift = (addr & 3u) * 8u;
    *out_value = (u8)(0x47704770u >> shift);
    return 1;
}

int emu_boot2_stub_read16(u32 addr, u16 *out_value) {
    u8 lo;
    u8 hi;
    if ((addr & 1u) != 0u) return 0;
    if (!emu_boot2_stub_read8(addr, &lo)) return 0;
    if (!emu_boot2_stub_read8(addr + 1u, &hi)) return 0;
    *out_value = (u16)((u16)lo | ((u16)hi << 8));
    return 1;
}

int emu_boot2_stub_read32(u32 addr, u32 *out_value) {
    if (addr < BOOT2_BASE || addr >= BOOT2_BASE + BOOT2_SIZE || (addr & 3u) != 0u) return 0;
    *out_value = 0x47704770u;
    return 1;
}

int emu_bootrom_read8(u32 addr, u8 *out_value) {
    u32 value;
    u32 sf_off;
    if (addr == 0x13u) { *out_value = 1u; return 1; }
    if (addr >= BOOTROM_SD_TABLE - 2u && addr < BOOTROM_SD_TABLE + 128u) { *out_value = 0; return 1; }
    if (addr >= BOOTROM_SF_TABLE - 2u && addr < BOOTROM_SF_TABLE + 128u) {
        value = 0;
        if (addr >= BOOTROM_SF_TABLE) {
            sf_off = (addr - BOOTROM_SF_TABLE) & ~3u;
            if (sf_off == 0x00u) value = BOOTROM_FN_FADD | 1u;
            else if (sf_off == 0x04u) value = BOOTROM_FN_FSUB | 1u;
            else if (sf_off == 0x08u) value = BOOTROM_FN_FMUL | 1u;
            else if (sf_off == 0x0cu) value = BOOTROM_FN_FDIV | 1u;
            else if (sf_off == 0x24u) value = BOOTROM_FN_F2I | 1u;
        }
        *out_value = (u8)(value >> (((addr - BOOTROM_SF_TABLE) & 3u) * 8u));
        return 1;
    }
    if (addr >= 0x14u && addr < 0x1cu) {
        u32 base;
        if (addr < 0x16u) { value = BOOTROM_FUNC_TABLE; base = 0x14u; }
        else if (addr < 0x18u) { value = BOOTROM_DATA_TABLE; base = 0x16u; }
        else { value = BOOTROM_LOOKUP_ADDR | 1u; base = 0x18u; }
        *out_value = (u8)(value >> ((addr - base) * 8u));
        return 1;
    }
    return 0;
}

int emu_bootrom_read16(u32 addr, u16 *out_value) {
    u8 lo;
    u8 hi;
    if (!emu_bootrom_read8(addr, &lo)) return 0;
    if (!emu_bootrom_read8(addr + 1u, &hi)) return 0;
    *out_value = (u16)((u16)lo | ((u16)hi << 8));
    return 1;
}

int emu_bootrom_read32(u32 addr, u32 *out_value) {
    u16 lo;
    u16 hi;
    if (!emu_bootrom_read16(addr, &lo)) return 0;
    if (!emu_bootrom_read16(addr + 2u, &hi)) return 0;
    *out_value = (u32)lo | ((u32)hi << 16);
    return 1;
}

int emu_bootrom_lookup_call(EmuState *emu, Cpu *cpu, u32 return_pc) {
    u32 table = cpu->r[0];
    u32 code = cpu->r[1] & 0xffffu;
    u32 result = 0;
    if (table == BOOTROM_DATA_TABLE) {
        if (code == ROM_CODE_SD) result = BOOTROM_SD_TABLE;
        else if (code == ROM_CODE_SF) result = BOOTROM_SF_TABLE;
    } else if (table == BOOTROM_FUNC_TABLE) {
        if (code == ROM_CODE_L3) result = BOOTROM_FN_L3 | 1u;
        else if (code == ROM_CODE_P3) result = BOOTROM_FN_P3 | 1u;
        else if (code == ROM_CODE_R3) result = BOOTROM_FN_R3 | 1u;
        else if (code == ROM_CODE_T3) result = BOOTROM_FN_T3 | 1u;
        else if (code == ROM_CODE_MS) result = BOOTROM_FN_MS | 1u;
        else if (code == ROM_CODE_MC) result = BOOTROM_FN_MC | 1u;
        else if (code == ROM_CODE_S4) result = BOOTROM_FN_S4 | 1u;
        else if (code == ROM_CODE_C4) result = BOOTROM_FN_C4 | 1u;
        else if (code == ROM_CODE_IF) result = BOOTROM_FN_IF | 1u;
        else if (code == ROM_CODE_EX) result = BOOTROM_FN_EX | 1u;
        else if (code == ROM_CODE_FC) result = BOOTROM_FN_FC | 1u;
        else if (code == ROM_CODE_RE) result = BOOTROM_FN_RE | 1u;
        else if (code == ROM_CODE_RP) result = BOOTROM_FN_RP | 1u;
    }
    cpu->r[0] = result;
    cpu->r[15] = return_pc;
    if (emu_trace_enabled(emu, TRACE_CALLS)) {
        emu_trace_text(emu, "bootrom lookup table="); emu_trace_hex32(emu, table);
        emu_trace_text(emu, " code="); emu_trace_hex32(emu, code);
        emu_trace_text(emu, " result="); emu_trace_hex32(emu, result);
        emu_trace_text(emu, "\n");
    }
    return 1;
}

int emu_bootrom_function_call(EmuState *emu, Cpu *cpu, u32 target, u32 return_pc, const EmuBootromMemOps *mem, void *mem_ctx) {
    u32 dst;
    u32 src;
    u32 len;
    u32 i;
    if (target == BOOTROM_FN_L3) cpu->r[0] = count_leading_zeroes(cpu->r[0]);
    else if (target == BOOTROM_FN_P3) cpu->r[0] = count_bits(cpu->r[0]);
    else if (target == BOOTROM_FN_R3) cpu->r[0] = reverse_bits(cpu->r[0]);
    else if (target == BOOTROM_FN_T3) cpu->r[0] = count_trailing_zeroes(cpu->r[0]);
    else if (target == BOOTROM_FN_MS || target == BOOTROM_FN_S4) {
        dst = cpu->r[0];
        len = cpu->r[2];
        if (target == BOOTROM_FN_S4) {
            for (i = 0; i < len; i += 4u) mem->write32(mem_ctx, dst + i, cpu->r[1]);
        } else {
            for (i = 0; i < len; ++i) mem->write8(mem_ctx, dst + i, cpu->r[1]);
        }
    } else if (target == BOOTROM_FN_MC || target == BOOTROM_FN_C4) {
        dst = cpu->r[0];
        src = cpu->r[1];
        len = cpu->r[2];
        if (target == BOOTROM_FN_C4) {
            for (i = 0; i < len; i += 4u) mem->write32(mem_ctx, dst + i, mem->read32(mem_ctx, src + i));
        } else {
            for (i = 0; i < len; ++i) mem->write8(mem_ctx, dst + i, mem->read8(mem_ctx, src + i));
        }
    } else if (target == BOOTROM_FN_RE) {
        usize flash_off = (usize)cpu->r[0];
        len = cpu->r[1];
        if (flash_off < sizeof(emu->flash)) {
            usize limit = flash_off + len;
            if (limit > sizeof(emu->flash) || limit < flash_off) limit = sizeof(emu->flash);
            for (i = (u32)flash_off; i < limit; ++i) emu->flash[i] = 0xffu;
        }
        cpu->r[0] = 0;
    } else if (target == BOOTROM_FN_RP) {
        usize flash_off = (usize)cpu->r[0];
        src = cpu->r[1];
        len = cpu->r[2];
        if (flash_off < sizeof(emu->flash)) {
            usize limit = flash_off + len;
            if (limit > sizeof(emu->flash) || limit < flash_off) limit = sizeof(emu->flash);
            for (i = 0; flash_off + i < limit; ++i) emu->flash[flash_off + i] &= mem->read8(mem_ctx, src + i);
        }
        cpu->r[0] = 0;
    } else if (target == BOOTROM_FN_IF || target == BOOTROM_FN_EX || target == BOOTROM_FN_FC) {
        cpu->r[0] = 0;
    } else if (target == BOOTROM_FN_FADD) {
        cpu->r[0] = float_to_bits(bits_to_float(cpu->r[0]) + bits_to_float(cpu->r[1]));
    } else if (target == BOOTROM_FN_FSUB) {
        cpu->r[0] = float_to_bits(bits_to_float(cpu->r[0]) - bits_to_float(cpu->r[1]));
    } else if (target == BOOTROM_FN_FMUL) {
        cpu->r[0] = float_to_bits(bits_to_float(cpu->r[0]) * bits_to_float(cpu->r[1]));
    } else if (target == BOOTROM_FN_FDIV) {
        cpu->r[0] = float_to_bits(bits_to_float(cpu->r[0]) / bits_to_float(cpu->r[1]));
    } else if (target == BOOTROM_FN_F2I) {
        cpu->r[0] = (u32)(s32)bits_to_float(cpu->r[0]);
    } else return 0;
    cpu->r[15] = return_pc;
    if (emu_trace_enabled(emu, TRACE_CALLS)) {
        emu_trace_text(emu, "bootrom fn target="); emu_trace_hex32(emu, target);
        emu_trace_text(emu, " r0="); emu_trace_hex32(emu, cpu->r[0]);
        emu_trace_text(emu, " r1="); emu_trace_hex32(emu, cpu->r[1]);
        emu_trace_text(emu, " r2="); emu_trace_hex32(emu, cpu->r[2]);
        emu_trace_text(emu, " return="); emu_trace_hex32(emu, return_pc);
        emu_trace_text(emu, "\n");
    }
    return 1;
}