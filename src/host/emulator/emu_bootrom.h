#ifndef PICOCALC_EMU_BOOTROM_H
#define PICOCALC_EMU_BOOTROM_H

#include "emu_state.h"

#define BOOTROM_LOOKUP_ADDR    0x00000100u

typedef struct {
    u8 (*read8)(void *ctx, u32 addr);
    u32 (*read32)(void *ctx, u32 addr);
    void (*write8)(void *ctx, u32 addr, u32 value);
    void (*write32)(void *ctx, u32 addr, u32 value);
} EmuBootromMemOps;

int emu_boot2_stub_read8(u32 addr, u8 *out_value);
int emu_boot2_stub_read16(u32 addr, u16 *out_value);
int emu_boot2_stub_read32(u32 addr, u32 *out_value);
int emu_bootrom_read8(u32 addr, u8 *out_value);
int emu_bootrom_read16(u32 addr, u16 *out_value);
int emu_bootrom_read32(u32 addr, u32 *out_value);
int emu_bootrom_lookup_call(EmuState *emu, Cpu *cpu, u32 return_pc);
int emu_bootrom_function_call(EmuState *emu, Cpu *cpu, u32 target, u32 return_pc, const EmuBootromMemOps *mem, void *mem_ctx);

#endif