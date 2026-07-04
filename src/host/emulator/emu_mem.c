#include "emu_mem.h"
#include "emu_bootrom.h"
#include "emu_util.h"

#define FLASH_BASE 0x10000000u
#define APP_BASE   0x10032000u
#define APP_FLASH_OFFSET (APP_BASE - FLASH_BASE)
#define RAM_BASE   0x20000000u

int emu_mem_flash_offset(EmuState *emu, u32 addr, usize *out_off) {
    (void)emu;
    if (addr < FLASH_BASE) return 0;
    *out_off = (usize)(addr - FLASH_BASE);
    return *out_off < sizeof(emu->flash);
}

int emu_mem_ram_offset(EmuState *emu, u32 addr, usize *out_off) {
    (void)emu;
    if (addr < RAM_BASE) return 0;
    *out_off = (usize)(addr - RAM_BASE);
    return *out_off < RAM_SIZE;
}

int emu_mem_executable_addr(EmuState *emu, u32 addr) {
    usize off;
    if ((addr & 1u) != 0u) return 0;
    if (emu_mem_flash_offset(emu, addr, &off) && off + 2u <= sizeof(emu->flash)) return 1;
    if (emu_mem_ram_offset(emu, addr, &off) && off + 2u <= RAM_SIZE) return 1;
    return 0;
}

u32 emu_mem_read32(EmuState *emu, const EmuMemMmioOps *mmio, void *mmio_ctx, u32 addr) {
    usize off;
    u32 value;
    if (emu_mem_ram_offset(emu, addr, &off) && off + 4u <= RAM_SIZE) return read_le32(emu->ram + off);
    if (emu_boot2_stub_read32(addr, &value)) return value;
    if (emu_mem_flash_offset(emu, addr, &off) && off + 4u <= sizeof(emu->flash)) return read_le32(emu->flash + off);
    if (emu_bootrom_read32(addr, &value)) return value;
    return mmio->read32(mmio_ctx, addr);
}

u16 emu_mem_read16(EmuState *emu, const EmuMemMmioOps *mmio, void *mmio_ctx, u32 addr) {
    usize off;
    u16 value;
    if (emu_mem_ram_offset(emu, addr, &off) && off + 2u <= RAM_SIZE) return read_le16(emu->ram + off);
    if (emu_boot2_stub_read16(addr, &value)) return value;
    if (emu_mem_flash_offset(emu, addr, &off) && off + 2u <= sizeof(emu->flash)) return read_le16(emu->flash + off);
    if (emu_bootrom_read16(addr, &value)) return value;
    return (u16)emu_mem_read32(emu, mmio, mmio_ctx, addr);
}

u8 emu_mem_read8(EmuState *emu, const EmuMemMmioOps *mmio, void *mmio_ctx, u32 addr) {
    usize off;
    u8 value;
    if (emu_mem_ram_offset(emu, addr, &off) && off < RAM_SIZE) return emu->ram[off];
    if (emu_boot2_stub_read8(addr, &value)) return value;
    if (emu_mem_flash_offset(emu, addr, &off) && off < sizeof(emu->flash)) return emu->flash[off];
    if (emu_bootrom_read8(addr, &value)) return value;
    return (u8)emu_mem_read32(emu, mmio, mmio_ctx, addr);
}

void emu_mem_write32(EmuState *emu, const EmuMemMmioOps *mmio, void *mmio_ctx, u32 addr, u32 value) {
    usize off;
    if (emu_mem_ram_offset(emu, addr, &off) && off + 4u <= RAM_SIZE) {
        write_le32(emu->ram + off, value);
        return;
    }
    mmio->write32(mmio_ctx, addr, value);
}

void emu_mem_write16(EmuState *emu, const EmuMemMmioOps *mmio, void *mmio_ctx, u32 addr, u32 value) {
    usize off;
    if (emu_mem_ram_offset(emu, addr, &off) && off + 2u <= RAM_SIZE) {
        emu->ram[off] = (u8)value;
        emu->ram[off + 1u] = (u8)(value >> 8);
        return;
    }
    mmio->write32(mmio_ctx, addr, value & 0xffffu);
}

void emu_mem_write8(EmuState *emu, const EmuMemMmioOps *mmio, void *mmio_ctx, u32 addr, u32 value) {
    usize off;
    if (emu_mem_ram_offset(emu, addr, &off) && off < RAM_SIZE) {
        emu->ram[off] = (u8)value;
        return;
    }
    mmio->write32(mmio_ctx, addr, value & 0xffu);
}

void emu_mem_flash_fill_erased(EmuState *emu) {
    usize i;
    for (i = 0; i < sizeof(emu->flash); ++i) emu->flash[i] = 0xffu;
    emu->flash_size = APP_FLASH_OFFSET;
}