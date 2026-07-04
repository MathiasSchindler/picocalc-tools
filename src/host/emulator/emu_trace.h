#ifndef PICOCALC_EMU_TRACE_H
#define PICOCALC_EMU_TRACE_H

#include "emu_state.h"

#define TRACE_BASE             (1u << 0)
#define TRACE_CALLS            (1u << 1)
#define TRACE_UNKNOWN_MMIO     (1u << 2)
#define TRACE_XIP              (1u << 3)

int emu_trace_parse_kinds(const char *s, u32 *out_mask);
void emu_trace_text(EmuState *emu, const char *s);
int emu_trace_enabled(EmuState *emu, u32 kind);
void emu_trace_hex32(EmuState *emu, u32 value);
void emu_trace_pair(EmuState *emu, const char *name, u32 value);
void emu_trace_mmio(EmuState *emu, const char *kind, u32 addr, u32 value);
void emu_trace_unknown_mmio(EmuState *emu, const char *kind, u32 addr, u32 value, u32 pc);
void emu_trace_xip_mmio(EmuState *emu, const char *kind, u32 addr, u32 value);
void emu_trace_branch(EmuState *emu, const char *kind, u32 pc, u32 target, u32 lr, u16 op);

#endif