#ifndef PICOCALC_EMU_MEM_H
#define PICOCALC_EMU_MEM_H

#include "emu_state.h"

typedef struct {
    u32 (*read32)(void *ctx, u32 addr);
    void (*write8)(void *ctx, u32 addr, u32 value);
    void (*write16)(void *ctx, u32 addr, u32 value);
    void (*write32)(void *ctx, u32 addr, u32 value);
} EmuMemMmioOps;

int emu_mem_flash_offset(EmuState *emu, u32 addr, usize *out_off);
int emu_mem_ram_offset(EmuState *emu, u32 addr, usize *out_off);
int emu_mem_executable_addr(EmuState *emu, u32 addr);
u32 emu_mem_read32(EmuState *emu, const EmuMemMmioOps *mmio, void *mmio_ctx, u32 addr);
u16 emu_mem_read16(EmuState *emu, const EmuMemMmioOps *mmio, void *mmio_ctx, u32 addr);
u8 emu_mem_read8(EmuState *emu, const EmuMemMmioOps *mmio, void *mmio_ctx, u32 addr);
void emu_mem_write32(EmuState *emu, const EmuMemMmioOps *mmio, void *mmio_ctx, u32 addr, u32 value);
void emu_mem_write16(EmuState *emu, const EmuMemMmioOps *mmio, void *mmio_ctx, u32 addr, u32 value);
void emu_mem_write8(EmuState *emu, const EmuMemMmioOps *mmio, void *mmio_ctx, u32 addr, u32 value);
void emu_mem_flash_fill_erased(EmuState *emu);

#endif