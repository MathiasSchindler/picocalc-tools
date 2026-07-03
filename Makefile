CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -O2 -ffreestanding -fno-builtin -fno-pie -fno-stack-protector -DPICOCALC_SOLVE_PROVIDE_MEMOPS -Isrc
LDFLAGS ?= -nostdlib -no-pie
LDLIBS ?= -lgcc

BUILD_DIR := build
HOST_SOLVE := $(BUILD_DIR)/solve-host
HOST_OBJS := $(BUILD_DIR)/solve.o $(BUILD_DIR)/support.o $(BUILD_DIR)/platform_linux_x86_64.o
HOST_SOLVE_SRC_DIR := src/host/solve
HOST_EMU_SRC_DIR := src/host/emulator
HOST_SIM_SRC_DIR := src/host/sim
HOST_TOOLS_SRC_DIR := src/host/tools
PICOCALC_BARE_SRC_DIR := src/picocalc/bare
FONT_DIR := $(BUILD_DIR)/font
FONTGEN := $(FONT_DIR)/gen_picocalc_font
FONTRENDER_DIR := vendor/newos/fontrender
FONTGEN_CFLAGS ?= -std=c11 -Wall -Wextra -O2 -I$(FONTRENDER_DIR)
FONTGEN_SRCS := $(HOST_TOOLS_SRC_DIR)/gen_picocalc_font.c $(FONTRENDER_DIR)/fr_platform.c $(FONTRENDER_DIR)/fr_ttf.c $(FONTRENDER_DIR)/fr_raster.c $(FONTRENDER_DIR)/font_backend_truetype.c
FONTGEN_INPUT := vendor/microsoft/cascadia/CascadiaMono.ttf
FONTGEN_HEADER := $(FONT_DIR)/picocalc_cascadia_8x14.h
FONTGEN_PREVIEW := $(FONT_DIR)/picocalc_cascadia_8x14.ppm
EMU_DIR := $(BUILD_DIR)/emu
BIN_EMU := $(EMU_DIR)/bin_emu
EMU_SOLVE_REPLAY := tests/solve_replay.keys
EMU_HASH_HELLO := 0x8ff880ad
EMU_HASH_GRAPHICS := 0x401ffcba
EMU_HASH_SOLVE := 0x34c38b9e
EMU_HASH_INTERRUPT := 0x9a855e0b
EMU_HASH_DMA := 0x6837fa02
EMU_HASH_THUMB := 0x32d38172
EMU_HASH_VENDOR_STARTUP := 0x6867f247
EMU_VENDOR_DIR := $(BUILD_DIR)/vendor-emu
VENDOR_IMAGE_BINS := vendor/images/Lua_180a58e.bin vendor/images/MicroPython_fa8b24c.bin vendor/images/MP3player_v0.5.bin vendor/images/PicoCalc_NES_v1.0.bin vendor/images/PicoMite_cbf6d71.bin vendor/images/uLisp_4.8f.bin
CUBE_GIF_FRAMES ?= 45
CUBE_GIF_FPS ?= 15
CUBE_GIF_MAX_STEPS ?= 80000000
PNG_WRITER_OBJ := $(EMU_DIR)/png_writer.o
GIF_WRITER_OBJ := $(EMU_DIR)/gif_writer.o
BIN_EMU_OBJS := $(EMU_DIR)/bin_emu.o $(PNG_WRITER_OBJ) $(GIF_WRITER_OBJ)
GUI_DIR := $(BUILD_DIR)/gui
PICOCALC_SHELL := $(EMU_DIR)/picocalc_shell
PICOCALC_SHELL_OBJS := $(EMU_DIR)/picocalc_shell.o $(PNG_WRITER_OBJ)
EMU_CFLAGS ?= -std=c11 -Wall -Wextra -O2 -ffreestanding -fno-builtin -fno-pie -fno-stack-protector -I$(HOST_EMU_SRC_DIR)
EMU_LDFLAGS ?= -nostdlib -no-pie
SIM_DIR := $(BUILD_DIR)/sim
SIM_SOLVE_FIXED := $(SIM_DIR)/sim_solve_fixed
SIM_SOLVE_REPL := $(SIM_DIR)/sim_solve_repl
SIM_GRAPHICS := $(SIM_DIR)/sim_graphics
SIM_CFLAGS ?= -std=c11 -Wall -Wextra -O2 -ffreestanding -fno-builtin -fno-pie -fno-stack-protector -DPICOCALC_BARE_SIM -DPICOCALC_SOLVE_PROVIDE_MEMOPS -Isrc -I$(PICOCALC_BARE_SRC_DIR) -I$(HOST_SIM_SRC_DIR) -I$(FONT_DIR)
SIM_LDFLAGS ?= -nostdlib -no-pie
SIM_LDLIBS ?= -lgcc
SIM_SOLVE_FIXED_OBJS := $(SIM_DIR)/host_main.o $(SIM_DIR)/picocalc_lcd_sim.o $(SIM_DIR)/solve_fixed.o $(SIM_DIR)/solve.o $(SIM_DIR)/support.o
SIM_SOLVE_REPL_OBJS := $(SIM_DIR)/host_main.o $(SIM_DIR)/picocalc_lcd_sim.o $(SIM_DIR)/solve_repl.o $(SIM_DIR)/solve_repl_input.o $(SIM_DIR)/solve.o $(SIM_DIR)/support.o
SIM_GRAPHICS_OBJS := $(SIM_DIR)/host_main.o $(SIM_DIR)/picocalc_lcd_sim.o $(SIM_DIR)/graphics_demo.o
BARE_CC ?= arm-none-eabi-gcc
BARE_OBJCOPY ?= arm-none-eabi-objcopy
BARE_SIZE ?= arm-none-eabi-size
BARE_DIR := $(BUILD_DIR)/bare
BARE_HELLO_ELF := $(BARE_DIR)/bare_hello.elf
BARE_HELLO_BIN := $(BARE_DIR)/bare_hello.bin
BARE_KEYS_ELF := $(BARE_DIR)/bare_keys.elf
BARE_KEYS_BIN := $(BARE_DIR)/bare_keys.bin
BARE_SOLVE_FIXED_ELF := $(BARE_DIR)/bare_solve_fixed.elf
BARE_SOLVE_FIXED_BIN := $(BARE_DIR)/bare_solve_fixed.bin
BARE_SOLVE_ELF := $(BARE_DIR)/bare_solve.elf
BARE_SOLVE_BIN := $(BARE_DIR)/bare_solve.bin
BARE_GRAPHICS_ELF := $(BARE_DIR)/bare_graphics.elf
BARE_GRAPHICS_BIN := $(BARE_DIR)/bare_graphics.bin
BARE_CUBE_ELF := $(BARE_DIR)/bare_cube.elf
BARE_CUBE_BIN := $(BARE_DIR)/bare_cube.bin
BARE_BENCHMARK_ELF := $(BARE_DIR)/bare_benchmark.elf
BARE_BENCHMARK_BIN := $(BARE_DIR)/bare_benchmark.bin
BARE_DIAGNOSTICS_ELF := $(BARE_DIR)/bare_diagnostics.elf
BARE_DIAGNOSTICS_BIN := $(BARE_DIR)/bare_diagnostics.bin
BARE_INTERRUPT_ELF := $(BARE_DIR)/bare_interrupt_probe.elf
BARE_INTERRUPT_BIN := $(BARE_DIR)/bare_interrupt_probe.bin
BARE_DMA_ELF := $(BARE_DIR)/bare_dma_probe.elf
BARE_DMA_BIN := $(BARE_DIR)/bare_dma_probe.bin
BARE_THUMB_ELF := $(BARE_DIR)/bare_thumb_probe.elf
BARE_THUMB_BIN := $(BARE_DIR)/bare_thumb_probe.bin
BARE_VENDOR_STARTUP_ELF := $(BARE_DIR)/bare_vendor_startup_probe.elf
BARE_VENDOR_STARTUP_BIN := $(BARE_DIR)/bare_vendor_startup_probe.bin
BARE_CFLAGS ?= -std=c11 -Wall -Wextra -Os -ffreestanding -fno-builtin -fno-stack-protector -ffunction-sections -fdata-sections -mcpu=cortex-m0plus -mthumb -I$(PICOCALC_BARE_SRC_DIR) -Isrc -I$(FONT_DIR)
BARE_LDFLAGS ?= -nostdlib -Wl,--gc-sections -T$(PICOCALC_BARE_SRC_DIR)/memmap_sd_rp2040.ld
BARE_LDLIBS ?= -lgcc
BARE_HELLO_OBJS := $(BARE_DIR)/start.o $(BARE_DIR)/picocalc_lcd_bare.o $(BARE_DIR)/hello.o
BARE_KEYS_OBJS := $(BARE_DIR)/start.o $(BARE_DIR)/picocalc_lcd_bare.o $(BARE_DIR)/picocalc_kbd_bare.o $(BARE_DIR)/keys.o
BARE_SOLVE_FIXED_OBJS := $(BARE_DIR)/start.o $(BARE_DIR)/picocalc_lcd_bare.o $(BARE_DIR)/solve_fixed.o $(BARE_DIR)/solve.o $(BARE_DIR)/support.o
BARE_SOLVE_OBJS := $(BARE_DIR)/start.o $(BARE_DIR)/picocalc_lcd_bare.o $(BARE_DIR)/picocalc_kbd_bare.o $(BARE_DIR)/solve_repl.o $(BARE_DIR)/solve_repl_bare_input.o $(BARE_DIR)/solve.o $(BARE_DIR)/support.o
BARE_GRAPHICS_OBJS := $(BARE_DIR)/start.o $(BARE_DIR)/picocalc_lcd_bare.o $(BARE_DIR)/graphics_demo.o
BARE_CUBE_OBJS := $(BARE_DIR)/start.o $(BARE_DIR)/picocalc_lcd_bare.o $(BARE_DIR)/picocalc_kbd_bare.o $(BARE_DIR)/cube.o
BARE_BENCHMARK_OBJS := $(BARE_DIR)/start.o $(BARE_DIR)/picocalc_lcd_bare.o $(BARE_DIR)/benchmark.o
BARE_DIAGNOSTICS_OBJS := $(BARE_DIR)/start.o $(BARE_DIR)/picocalc_lcd_bare.o $(BARE_DIR)/picocalc_kbd_bare.o $(BARE_DIR)/diagnostics.o
BARE_INTERRUPT_OBJS := $(BARE_DIR)/start.o $(BARE_DIR)/picocalc_lcd_bare.o $(BARE_DIR)/interrupt_probe.o
BARE_DMA_OBJS := $(BARE_DIR)/start.o $(BARE_DIR)/picocalc_lcd_bare.o $(BARE_DIR)/dma_probe.o
BARE_THUMB_OBJS := $(BARE_DIR)/start.o $(BARE_DIR)/picocalc_lcd_bare.o $(BARE_DIR)/thumb_probe.o
BARE_VENDOR_STARTUP_OBJS := $(BARE_DIR)/start.o $(BARE_DIR)/picocalc_lcd_bare.o $(BARE_DIR)/vendor_startup_probe.o

.PHONY: all arm-probe bare-benchmark bare-cube bare-diagnostics bare-dma-probe bare-graphics bare-hello bare-interrupt-probe bare-keys bare-solve bare-solve-fixed bare-thumb-probe bare-vendor-startup-probe bin-emu-benchmark bin-emu-cube bin-emu-cube-gif bin-emu-diagnostics bin-emu-dma-probe bin-emu-graphics bin-emu-graphics-frames bin-emu-hello bin-emu-hello-trace bin-emu-interrupt-probe bin-emu-live-hello bin-emu-live-solve bin-emu-solve bin-emu-thumb-probe bin-emu-vendor-startup-probe clean emu-deterministic-tests emu-replay-manifest-check emu-vendor-probe font-cascadia gui-graphics gui-hello gui-solve sim-graphics sim-solve-fixed sim-solve-repl smoke

all: $(HOST_SOLVE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/platform_linux_x86_64.o: $(HOST_SOLVE_SRC_DIR)/platform_linux_x86_64.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(HOST_SOLVE): $(HOST_OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(FONT_DIR):
	mkdir -p $(FONT_DIR)

$(FONTGEN): $(FONTGEN_SRCS) | $(FONT_DIR)
	$(CC) $(FONTGEN_CFLAGS) $(FONTGEN_SRCS) -o $@

$(FONTGEN_HEADER) $(FONTGEN_PREVIEW) &: $(FONTGEN) $(FONTGEN_INPUT) | $(FONT_DIR)
	$(FONTGEN) $(FONTGEN_INPUT) $(FONTGEN_HEADER) $(FONTGEN_PREVIEW)

font-cascadia: $(FONTGEN_HEADER) $(FONTGEN_PREVIEW)

$(EMU_DIR):
	mkdir -p $(EMU_DIR)

$(EMU_DIR)/%.o: $(HOST_EMU_SRC_DIR)/%.c | $(EMU_DIR)
	$(CC) $(EMU_CFLAGS) -c $< -o $@

$(BIN_EMU): $(BIN_EMU_OBJS)
	$(CC) $(EMU_LDFLAGS) $^ -lgcc -o $@

$(PICOCALC_SHELL): $(PICOCALC_SHELL_OBJS)
	$(CC) $(EMU_LDFLAGS) $^ -lgcc -o $@

$(GUI_DIR):
	mkdir -p $(GUI_DIR)

bin-emu-hello: $(BIN_EMU) $(BARE_HELLO_BIN)
	$(BIN_EMU) $(BARE_HELLO_BIN) $(EMU_DIR)/bare_hello.png

bin-emu-hello-trace: $(BIN_EMU) $(BARE_HELLO_BIN)
	$(BIN_EMU) $(BARE_HELLO_BIN) $(EMU_DIR)/bare_hello.png --trace=$(EMU_DIR)/bare_hello.trace

bin-emu-graphics: $(BIN_EMU) $(BARE_GRAPHICS_BIN)
	$(BIN_EMU) $(BARE_GRAPHICS_BIN) $(EMU_DIR)/bare_graphics.png

bin-emu-graphics-frames: $(BIN_EMU) $(BARE_GRAPHICS_BIN)
	$(BIN_EMU) $(BARE_GRAPHICS_BIN) '$(EMU_DIR)/bare_graphics_frame_%d.png' --frames=2

bin-emu-cube: $(BIN_EMU) $(BARE_CUBE_BIN)
	$(BIN_EMU) $(BARE_CUBE_BIN) $(EMU_DIR)/bare_cube.png

bin-emu-cube-gif: $(BIN_EMU) $(BARE_CUBE_BIN)
	$(BIN_EMU) $(BARE_CUBE_BIN) $(EMU_DIR)/bare_cube.gif --frames=$(CUBE_GIF_FRAMES) --gif-fps=$(CUBE_GIF_FPS) --max-steps=$(CUBE_GIF_MAX_STEPS)

bin-emu-solve: $(BIN_EMU) $(BARE_SOLVE_BIN)
	$(BIN_EMU) $(BARE_SOLVE_BIN) $(EMU_DIR)/bare_solve.png '6x-3=0\n\x04'

bin-emu-benchmark: $(BIN_EMU) $(BARE_BENCHMARK_BIN)
	$(BIN_EMU) $(BARE_BENCHMARK_BIN) $(EMU_DIR)/bare_benchmark.png

bin-emu-diagnostics: $(BIN_EMU) $(BARE_DIAGNOSTICS_BIN)
	$(BIN_EMU) $(BARE_DIAGNOSTICS_BIN) $(EMU_DIR)/bare_diagnostics.png

bin-emu-interrupt-probe: $(BIN_EMU) $(BARE_INTERRUPT_BIN)
	$(BIN_EMU) $(BARE_INTERRUPT_BIN) $(EMU_DIR)/bare_interrupt_probe.png --trace=$(EMU_DIR)/bare_interrupt_probe.trace

bin-emu-dma-probe: $(BIN_EMU) $(BARE_DMA_BIN)
	$(BIN_EMU) $(BARE_DMA_BIN) $(EMU_DIR)/bare_dma_probe.png --trace=$(EMU_DIR)/bare_dma_probe.trace

bin-emu-thumb-probe: $(BIN_EMU) $(BARE_THUMB_BIN)
	$(BIN_EMU) $(BARE_THUMB_BIN) $(EMU_DIR)/bare_thumb_probe.png

bin-emu-vendor-startup-probe: $(BIN_EMU) $(BARE_VENDOR_STARTUP_BIN)
	$(BIN_EMU) $(BARE_VENDOR_STARTUP_BIN) $(EMU_DIR)/bare_vendor_startup_probe.png --trace=$(EMU_DIR)/bare_vendor_startup_probe.trace --trace-kinds=base,calls,unknown-mmio

bin-emu-live-hello: $(BIN_EMU) $(BARE_HELLO_BIN)
	$(BIN_EMU) $(BARE_HELLO_BIN) $(EMU_DIR)/bare_hello_live.png --live-terminal

bin-emu-live-solve: $(BIN_EMU) $(BARE_SOLVE_BIN)
	$(BIN_EMU) $(BARE_SOLVE_BIN) $(EMU_DIR)/bare_solve_live.png - --live-terminal

emu-replay-manifest-check: Makefile tests/emu_replays.tsv tests/check_emu_replays.awk
	awk -f tests/check_emu_replays.awk Makefile tests/emu_replays.tsv

emu-deterministic-tests: emu-replay-manifest-check $(BIN_EMU) $(BARE_HELLO_BIN) $(BARE_GRAPHICS_BIN) $(BARE_SOLVE_BIN) $(BARE_INTERRUPT_BIN) $(BARE_DMA_BIN) $(BARE_THUMB_BIN) $(BARE_VENDOR_STARTUP_BIN) $(EMU_SOLVE_REPLAY)
	$(BIN_EMU) $(BARE_HELLO_BIN) $(EMU_DIR)/test_hello.png --expect-hash=$(EMU_HASH_HELLO)
	$(BIN_EMU) $(BARE_GRAPHICS_BIN) $(EMU_DIR)/test_graphics.png --expect-hash=$(EMU_HASH_GRAPHICS)
	$(BIN_EMU) $(BARE_SOLVE_BIN) $(EMU_DIR)/test_solve.png @$(EMU_SOLVE_REPLAY) --expect-hash=$(EMU_HASH_SOLVE)
	$(BIN_EMU) $(BARE_INTERRUPT_BIN) $(EMU_DIR)/test_interrupt.png --trace=$(EMU_DIR)/test_interrupt.trace --expect-hash=$(EMU_HASH_INTERRUPT)
	$(BIN_EMU) $(BARE_DMA_BIN) $(EMU_DIR)/test_dma.png --trace=$(EMU_DIR)/test_dma.trace --expect-hash=$(EMU_HASH_DMA)
	$(BIN_EMU) $(BARE_THUMB_BIN) $(EMU_DIR)/test_thumb.png --expect-hash=$(EMU_HASH_THUMB)
	$(BIN_EMU) $(BARE_VENDOR_STARTUP_BIN) $(EMU_DIR)/test_vendor_startup.png --trace=$(EMU_DIR)/test_vendor_startup.trace --trace-kinds=base,calls,unknown-mmio --expect-hash=$(EMU_HASH_VENDOR_STARTUP)

emu-vendor-probe: $(BIN_EMU) $(VENDOR_IMAGE_BINS)
	mkdir -p $(EMU_VENDOR_DIR)
	@set +e; for f in $(VENDOR_IMAGE_BINS); do \
		name=$$(basename "$$f" .bin); \
		echo "=== $$name ==="; \
		$(BIN_EMU) "$$f" "$(EMU_VENDOR_DIR)/$$name.png" --trace="$(EMU_VENDOR_DIR)/$$name.trace" --trace-kinds=base,calls,unknown-mmio,xip --max-steps=2000000 --fail-on-budget --report-milestones; \
		echo "$$name exit=$$?"; \
	done

gui-hello: $(BIN_EMU) $(BARE_HELLO_BIN) $(PICOCALC_SHELL) | $(GUI_DIR)
	$(BIN_EMU) $(BARE_HELLO_BIN) $(EMU_DIR)/bare_hello_screen.ppm
	$(PICOCALC_SHELL) $(EMU_DIR)/bare_hello_screen.ppm $(GUI_DIR)/bare_hello_picocalc.png

gui-graphics: $(BIN_EMU) $(BARE_GRAPHICS_BIN) $(PICOCALC_SHELL) | $(GUI_DIR)
	$(BIN_EMU) $(BARE_GRAPHICS_BIN) $(EMU_DIR)/bare_graphics_screen.ppm
	$(PICOCALC_SHELL) $(EMU_DIR)/bare_graphics_screen.ppm $(GUI_DIR)/bare_graphics_picocalc.png

gui-solve: $(BIN_EMU) $(BARE_SOLVE_BIN) $(PICOCALC_SHELL) | $(GUI_DIR)
	$(BIN_EMU) $(BARE_SOLVE_BIN) $(EMU_DIR)/bare_solve_screen.ppm '6x-3=0\n\x04'
	$(PICOCALC_SHELL) $(EMU_DIR)/bare_solve_screen.ppm $(GUI_DIR)/bare_solve_picocalc.png

$(SIM_DIR):
	mkdir -p $(SIM_DIR)

$(SIM_DIR)/%.o: $(HOST_SIM_SRC_DIR)/%.c | $(SIM_DIR)
	$(CC) $(SIM_CFLAGS) -c $< -o $@

$(SIM_DIR)/%.o: $(PICOCALC_BARE_SRC_DIR)/%.c | $(SIM_DIR)
	$(CC) $(SIM_CFLAGS) -c $< -o $@

$(SIM_DIR)/solve.o: src/solve.c | $(SIM_DIR)
	$(CC) $(SIM_CFLAGS) -c $< -o $@

$(SIM_DIR)/support.o: src/support.c | $(SIM_DIR)
	$(CC) $(SIM_CFLAGS) -c $< -o $@

$(SIM_SOLVE_FIXED): $(SIM_SOLVE_FIXED_OBJS)
	$(CC) $(SIM_LDFLAGS) $^ $(SIM_LDLIBS) -o $@

sim-solve-fixed: $(SIM_SOLVE_FIXED)
	$(SIM_SOLVE_FIXED)

$(SIM_SOLVE_REPL): $(SIM_SOLVE_REPL_OBJS)
	$(CC) $(SIM_LDFLAGS) $^ $(SIM_LDLIBS) -o $@

sim-solve-repl: $(SIM_SOLVE_REPL)
	$(SIM_SOLVE_REPL)

$(SIM_GRAPHICS): $(SIM_GRAPHICS_OBJS)
	$(CC) $(SIM_LDFLAGS) $^ $(SIM_LDLIBS) -o $@

sim-graphics: $(SIM_GRAPHICS)
	$(SIM_GRAPHICS)

smoke: $(HOST_SOLVE)
	$(HOST_SOLVE) 'x^2 - 2 = 0'
	$(HOST_SOLVE) --discuss 'x^3 - 3*x'

arm-probe:
	mkdir -p $(BUILD_DIR)/arm-probe
	clang --target=arm-none-eabi -mcpu=cortex-m0plus -mthumb -ffreestanding -fno-builtin -fno-stack-protector -Os -Isrc -c src/solve.c -o $(BUILD_DIR)/arm-probe/solve.o
	clang --target=arm-none-eabi -mcpu=cortex-m0plus -mthumb -ffreestanding -fno-builtin -fno-stack-protector -DPICOCALC_SOLVE_PROVIDE_MEMOPS -Os -Isrc -c src/support.c -o $(BUILD_DIR)/arm-probe/support.o
	size $(BUILD_DIR)/arm-probe/solve.o $(BUILD_DIR)/arm-probe/support.o

$(BARE_DIR):
	mkdir -p $(BARE_DIR)

$(BARE_DIR)/%.o: $(PICOCALC_BARE_SRC_DIR)/%.c | $(BARE_DIR)
	$(BARE_CC) $(BARE_CFLAGS) -c $< -o $@

$(BARE_DIR)/solve.o: src/solve.c | $(BARE_DIR)
	$(BARE_CC) $(BARE_CFLAGS) -c $< -o $@

$(BARE_DIR)/support.o: src/support.c | $(BARE_DIR)
	$(BARE_CC) $(BARE_CFLAGS) -DPICOCALC_SOLVE_PROVIDE_MEMOPS -c $< -o $@

$(SIM_DIR)/picocalc_lcd_sim.o $(BARE_DIR)/picocalc_lcd_bare.o: $(FONTGEN_HEADER) $(PICOCALC_BARE_SRC_DIR)/picocalc_font.h

$(BARE_HELLO_OBJS) $(BARE_KEYS_OBJS) $(BARE_SOLVE_FIXED_OBJS) $(BARE_SOLVE_OBJS) $(BARE_GRAPHICS_OBJS) $(BARE_CUBE_OBJS) $(BARE_BENCHMARK_OBJS) $(BARE_DIAGNOSTICS_OBJS) $(BARE_INTERRUPT_OBJS) $(BARE_DMA_OBJS) $(BARE_THUMB_OBJS) $(BARE_VENDOR_STARTUP_OBJS): $(PICOCALC_BARE_SRC_DIR)/rp2040_regs.h $(PICOCALC_BARE_SRC_DIR)/picocalc_lcd_bare.h $(PICOCALC_BARE_SRC_DIR)/picocalc_kbd_bare.h

$(BARE_HELLO_ELF): $(BARE_HELLO_OBJS) $(PICOCALC_BARE_SRC_DIR)/memmap_sd_rp2040.ld
	$(BARE_CC) $(BARE_CFLAGS) $(BARE_LDFLAGS) $(BARE_HELLO_OBJS) $(BARE_LDLIBS) -o $@

$(BARE_HELLO_BIN): $(BARE_HELLO_ELF)
	$(BARE_OBJCOPY) -O binary $< $@
	$(BARE_SIZE) $<
	od -An -tx4 -N8 $@

bare-hello: $(BARE_HELLO_BIN)

$(BARE_KEYS_ELF): $(BARE_KEYS_OBJS) $(PICOCALC_BARE_SRC_DIR)/memmap_sd_rp2040.ld
	$(BARE_CC) $(BARE_CFLAGS) $(BARE_LDFLAGS) $(BARE_KEYS_OBJS) $(BARE_LDLIBS) -o $@

$(BARE_KEYS_BIN): $(BARE_KEYS_ELF)
	$(BARE_OBJCOPY) -O binary $< $@
	$(BARE_SIZE) $<
	od -An -tx4 -N8 $@

bare-keys: $(BARE_KEYS_BIN)

$(BARE_SOLVE_FIXED_ELF): $(BARE_SOLVE_FIXED_OBJS) $(PICOCALC_BARE_SRC_DIR)/memmap_sd_rp2040.ld
	$(BARE_CC) $(BARE_CFLAGS) $(BARE_LDFLAGS) $(BARE_SOLVE_FIXED_OBJS) $(BARE_LDLIBS) -o $@

$(BARE_SOLVE_FIXED_BIN): $(BARE_SOLVE_FIXED_ELF)
	$(BARE_OBJCOPY) -O binary $< $@
	$(BARE_SIZE) $<
	od -An -tx4 -N8 $@

bare-solve-fixed: $(BARE_SOLVE_FIXED_BIN)

$(BARE_SOLVE_ELF): $(BARE_SOLVE_OBJS) $(PICOCALC_BARE_SRC_DIR)/memmap_sd_rp2040.ld
	$(BARE_CC) $(BARE_CFLAGS) $(BARE_LDFLAGS) $(BARE_SOLVE_OBJS) $(BARE_LDLIBS) -o $@

$(BARE_SOLVE_BIN): $(BARE_SOLVE_ELF)
	$(BARE_OBJCOPY) -O binary $< $@
	$(BARE_SIZE) $<
	od -An -tx4 -N8 $@

bare-solve: $(BARE_SOLVE_BIN)

$(BARE_GRAPHICS_ELF): $(BARE_GRAPHICS_OBJS) $(PICOCALC_BARE_SRC_DIR)/memmap_sd_rp2040.ld
	$(BARE_CC) $(BARE_CFLAGS) $(BARE_LDFLAGS) $(BARE_GRAPHICS_OBJS) $(BARE_LDLIBS) -o $@

$(BARE_GRAPHICS_BIN): $(BARE_GRAPHICS_ELF)
	$(BARE_OBJCOPY) -O binary $< $@
	$(BARE_SIZE) $<
	od -An -tx4 -N8 $@

bare-graphics: $(BARE_GRAPHICS_BIN)

$(BARE_CUBE_ELF): $(BARE_CUBE_OBJS) $(PICOCALC_BARE_SRC_DIR)/memmap_sd_rp2040.ld
	$(BARE_CC) $(BARE_CFLAGS) $(BARE_LDFLAGS) $(BARE_CUBE_OBJS) $(BARE_LDLIBS) -o $@

$(BARE_CUBE_BIN): $(BARE_CUBE_ELF)
	$(BARE_OBJCOPY) -O binary $< $@
	$(BARE_SIZE) $<
	od -An -tx4 -N8 $@

bare-cube: $(BARE_CUBE_BIN)

$(BARE_BENCHMARK_ELF): $(BARE_BENCHMARK_OBJS) $(PICOCALC_BARE_SRC_DIR)/memmap_sd_rp2040.ld
	$(BARE_CC) $(BARE_CFLAGS) $(BARE_LDFLAGS) $(BARE_BENCHMARK_OBJS) $(BARE_LDLIBS) -o $@

$(BARE_BENCHMARK_BIN): $(BARE_BENCHMARK_ELF)
	$(BARE_OBJCOPY) -O binary $< $@
	$(BARE_SIZE) $<
	od -An -tx4 -N8 $@

bare-benchmark: $(BARE_BENCHMARK_BIN)

$(BARE_DIAGNOSTICS_ELF): $(BARE_DIAGNOSTICS_OBJS) $(PICOCALC_BARE_SRC_DIR)/memmap_sd_rp2040.ld
	$(BARE_CC) $(BARE_CFLAGS) $(BARE_LDFLAGS) $(BARE_DIAGNOSTICS_OBJS) $(BARE_LDLIBS) -o $@

$(BARE_DIAGNOSTICS_BIN): $(BARE_DIAGNOSTICS_ELF)
	$(BARE_OBJCOPY) -O binary $< $@
	$(BARE_SIZE) $<
	od -An -tx4 -N8 $@

bare-diagnostics: $(BARE_DIAGNOSTICS_BIN)

$(BARE_INTERRUPT_ELF): $(BARE_INTERRUPT_OBJS) $(PICOCALC_BARE_SRC_DIR)/memmap_sd_rp2040.ld
	$(BARE_CC) $(BARE_CFLAGS) $(BARE_LDFLAGS) $(BARE_INTERRUPT_OBJS) $(BARE_LDLIBS) -o $@

$(BARE_INTERRUPT_BIN): $(BARE_INTERRUPT_ELF)
	$(BARE_OBJCOPY) -O binary $< $@
	$(BARE_SIZE) $<
	od -An -tx4 -N8 $@

bare-interrupt-probe: $(BARE_INTERRUPT_BIN)

$(BARE_DMA_ELF): $(BARE_DMA_OBJS) $(PICOCALC_BARE_SRC_DIR)/memmap_sd_rp2040.ld
	$(BARE_CC) $(BARE_CFLAGS) $(BARE_LDFLAGS) $(BARE_DMA_OBJS) $(BARE_LDLIBS) -o $@

$(BARE_DMA_BIN): $(BARE_DMA_ELF)
	$(BARE_OBJCOPY) -O binary $< $@
	$(BARE_SIZE) $<
	od -An -tx4 -N8 $@

bare-dma-probe: $(BARE_DMA_BIN)

$(BARE_THUMB_ELF): $(BARE_THUMB_OBJS) $(PICOCALC_BARE_SRC_DIR)/memmap_sd_rp2040.ld
	$(BARE_CC) $(BARE_CFLAGS) $(BARE_LDFLAGS) $(BARE_THUMB_OBJS) $(BARE_LDLIBS) -o $@

$(BARE_THUMB_BIN): $(BARE_THUMB_ELF)
	$(BARE_OBJCOPY) -O binary $< $@
	$(BARE_SIZE) $<
	od -An -tx4 -N8 $@

bare-thumb-probe: $(BARE_THUMB_BIN)

$(BARE_VENDOR_STARTUP_ELF): $(BARE_VENDOR_STARTUP_OBJS) $(PICOCALC_BARE_SRC_DIR)/memmap_sd_rp2040.ld
	$(BARE_CC) $(BARE_CFLAGS) $(BARE_LDFLAGS) $(BARE_VENDOR_STARTUP_OBJS) $(BARE_LDLIBS) -o $@

$(BARE_VENDOR_STARTUP_BIN): $(BARE_VENDOR_STARTUP_ELF)
	$(BARE_OBJCOPY) -O binary $< $@
	$(BARE_SIZE) $<
	od -An -tx4 -N8 $@

bare-vendor-startup-probe: $(BARE_VENDOR_STARTUP_BIN)

clean:
	rm -rf $(BUILD_DIR)