# PicoCalc Freestanding Firmware and Emulator

This project is an experiment in building freestanding, dependency-free, no-libc software for the PicoCalc device. The current hardware target is the RP2040-based PicoCalc, but the code is intended to stay narrow and explicit enough that future PicoCalc variants, such as RP2350-based boards, can be approached without changing the overall direction.

The primary C firmware path deliberately does not use the Pico SDK. Firmware is built as raw PicoCalc SD-bootloader `.bin` images linked at `0x10032000`, with local startup code, direct register access, small support shims, and device-specific LCD/keyboard drivers. The older Pico SDK route remains documented only as historical context for the original porting work; the active SDK-free path is the one exercised by the bare firmware and emulator targets below.

`solve` is the first real use case. The repository contains a local fork of `newos/src/tools/solve.c`, trimmed support code for the solver, SDK-free PicoCalc I/O, and a growing RP2040/PicoCalc emulator so development can iterate quickly before copying binaries to the actual device.

Most code in this repository is LLM-generated with GPT-5.5 and released under CC-0. Vendored third-party material keeps its own upstream license and should be treated separately.

The repository currently contains:

- `src/solve.c` and `src/solve/*.c`, copied from NewOS as the calculator solver use case.
- `src/runtime.h`, `src/tool_util.h`, and `src/support.c`, providing only the support API used by the solver.
- `src/bare/`, containing SDK-free RP2040 startup, PicoCalc LCD/keyboard code, and focused emulator probe firmware.
- `src/emulator/`, containing the no-libc Linux `.bin` emulator and PNG writer.
- `src/platform_linux_x86_64.c`, a tiny syscall-based host runner for smoke tests.
- `src/platform_rp2040.c` and `src/picocalc_io.c`, retained from the earlier Pico SDK port path.
- `vendor/docs/`, `vendor/images/`, `vendor/PicoCalc/`, and `vendor/microsoft/`, containing local reference material and test inputs that are ignored by Git.
- `vendor/newos/`, the vendored NewOS font-rendering support that remains trackable because it is wired into the generated bitmap font path.

Build and run the host smoke test with:

```sh
make smoke
```

## Legacy Pico SDK port

The earlier Pico SDK port is still present for comparison and historical continuity, but it is not the primary firmware path for this project. The active C firmware work is the SDK-free bare-metal path documented below. If you need to rebuild the legacy SDK output, use a local Pico SDK checkout:

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S . -B build-pico
cmake --build build-pico
```

With the Raspberry Pi installer layout used on this machine, that path is:

```sh
export PICO_SDK_PATH=/home/mathias/pico-sdk/pico/pico-sdk
```

Two firmware formats are generated:

- `build-pico/picocalc_solve.uf2` is for normal BOOTSEL/USB UF2 flashing.
- `build-pico/solve.bin` is linked for the PicoCalc SD bootloader.

For the active SDK-free path, you can check that the solver core compiles for Cortex-M0+:

```sh
make arm-probe
```

`src/platform_rp2040.c` uses weak I/O hooks:

- `picocalc_solve_io_init`
- `picocalc_solve_getchar_timeout_us`
- `picocalc_solve_putchar`

By default those hooks use Pico SDK stdio over USB/UART. `src/picocalc_io.c` overrides them with ClockworkPi's I2C keyboard and SPI LCD drivers from `vendor/PicoCalc/Code/picocalc_helloworld`.

The PicoCalc LCD output uses a buffered text console instead of the vendor driver's pixel-readback scroll path. Simple implicit multiplication is normalized for calculator-style input, so expressions such as `6x-3=0` and `2(x+1)=6` work.

## SDK-free firmware path

The first bare-metal proof target builds without the Pico SDK or libc:

```sh
make bare-hello
```

This writes `build/bare/bare_hello.bin`, a PicoCalc SD-bootloader-style raw binary linked at `0x10032000`. Copy it to the SD card's `firmware/` directory to test the direct-register LCD hello screen.

The next bare hardware test adds direct I2C1 keyboard polling:

```sh
make bare-keys
```

This writes `build/bare/bare_keys.bin`. It should show a `bare keys` screen and update the displayed key code when keys are pressed.

Observed PicoCalc keyboard controller codes include Shift `0xa2`, Ctrl `0xa5`, Alt `0xa1`, Left `0xb4`, Up `0xb5`, Down `0xb6`, and Right `0xb7`. Shift modifies printable letters before reporting them, for example `u` is `0x75` and Shift+`u` is `0x55`. Alt+Space toggles keyboard lighting, Alt+`,` darkens the screen, and Alt+`.` brightens it.

The first SDK-free solver proof runs one fixed expression through the real solver core and prints the result to the LCD:

```sh
make bare-solve-fixed
```

This writes `build/bare/bare_solve_fixed.bin`. It currently runs `6x-3=0`; on hardware it reports `x = 1/2`, zero residual, linear method, and zero iterations. It is a link/runtime proof before the interactive bare REPL.

The first source-level simulator target runs that same fixed bare solver entry path on Linux and renders the simulated LCD text in the terminal:

```sh
make sim-solve-fixed
```

This writes and runs `build/sim/sim_solve_fixed`. It also writes `build/sim/sim_solve_fixed.ppm`, a raw `320x320` RGB framebuffer dump of the simulated LCD. The simulator is a freestanding Linux executable: it links with `-nostdlib -no-pie`, enters at `_start`, uses direct Linux syscalls for output/exit/file writing, and uses the local support shim for memory/string routines.

The interactive no-libc source-level simulator uses the same LCD console and solver REPL code as the bare PicoCalc target:

```sh
make sim-solve-repl
```

This writes and runs `build/sim/sim_solve_repl`, reads expressions from stdin with the Linux `read` syscall, and writes `build/sim/sim_solve_repl.ppm`. Type `exit` or press Ctrl-D to leave the simulator.

The SDK-free interactive PicoCalc build is:

```sh
make bare-solve
```

This writes `build/bare/bare_solve.bin`, the file to copy to the PicoCalc SD card's `firmware/` directory for hardware testing.

The shared graphics test pattern exercises color order, gradients, clipping, checkerboard fills, text overlay, and a simple moving-box workload on hardware:

```sh
make sim-graphics
make bare-graphics
```

The simulator writes `build/sim/sim_graphics.ppm`. The PicoCalc build writes `build/bare/bare_graphics.bin`, which can be copied to the SD card's `firmware/` directory.

The first actual `.bin` emulator target runs SD-boot binaries directly on Linux without libc. It maps flash/RAM, interprets a growing ARMv6-M Thumb subset, models the PicoCalc LCD framebuffer, and writes mildly compressed PNG output:

```sh
make bin-emu-hello
make bin-emu-graphics
make bin-emu-solve
```

These write `build/emu/bare_hello.png`, `build/emu/bare_graphics.png`, and `build/emu/bare_solve.png`. The solve target runs `bare_solve.bin` with a scripted keyboard input of `6x-3=0`, Enter, then Ctrl-D. The emulator currently targets the known bare firmware layout; it is a milestone-5 seed, not yet a general RP2040 emulator. It now executes the firmware's SPI LCD path, I2C keyboard path, text renderer, delay loops, and libgcc division code instead of jumping over those routines by fixed helper address. The scripted solve path depends on Thumb condition codes, `ADR`, carry-producing shifts, byte/halfword extend and reverse operations, and libgcc division/remainder behavior, all of which are now modeled closely enough for the linear result screenshot to show `x = 0.5000000000 (1/2)` with zero residual. If an output path explicitly ends in `.ppm`, the emulator still writes legacy raw `P6` PPM; the GUI targets use that only as an internal screen handoff.

The emulator also has optional diagnostic and capture modes:

```sh
make bin-emu-hello-trace
make bin-emu-graphics-frames
make emu-deterministic-tests
build/emu/bin_emu build/bare/bare_solve.bin build/emu/bare_solve.png @keys.txt
build/emu/bin_emu build/bare/bare_solve.bin build/emu/bare_solve.png -
```

`--trace=path` writes compact transaction-level logs for LCD commands, I2C operations, DMA activity, exception entry/return, and frame hashes. `--trace-kinds=base,calls,unknown-mmio|all` can add indirect branch/Boot ROM helper traces and unmodeled MMIO diagnostics; `make emu-vendor-probe` enables those richer trace kinds. `--report-milestones` prints the first LCD command, first pixel write, and first nonblack pixel with PC/cycle context; the vendor probe enables it to quickly classify whether an image has reached display output. A key-script argument of `@path` replays keys from a file, while `-` reads live keys from nonblocking stdin. If the output image path contains `%d`, `--frames=N` writes numbered PNG frames such as `bare_graphics_frame_0.png` and `bare_graphics_frame_1.png`. `--expect-hash=HEX` verifies the final framebuffer hash and exits nonzero on drift; `make emu-deterministic-tests` uses that mode for hello, graphics, solve, interrupt, DMA, Thumb, and vendor-startup probe firmware. The replay manifest in `tests/emu_replays.tsv` records the binaries, key inputs, trace outputs, hash variables, and expected framebuffer hashes covered by that deterministic suite; `make emu-replay-manifest-check` verifies the manifest hashes against the Makefile.

Vendor PicoCalc SD-app images can be probed with:

```sh
make emu-vendor-probe
```

This runs the SD-app-style binaries in `vendor/images`, writes PNG/trace artifacts under `build/vendor-emu`, and keeps going after individual failures. It is an investigation target, not a passing compatibility suite yet. The current emulator gets those binaries through vector loading, early reset/clock setup, targeted Boot ROM helper lookup, SIO divider/spinlock setup, and selected RAM helper paths. MP3 currently exits cleanly; the other vendor images still stop before useful LCD output.

For a terminal-hosted live view, run one of:

```sh
make bin-emu-live-hello
make bin-emu-live-solve
```

`--live-terminal` renders the 320x320 framebuffer as an ANSI true-color terminal view and puts stdin into noncanonical nonblocking mode so typed bytes feed the existing PicoCalc keyboard path. This is useful for quick local interaction without SDL/X11/Wayland, but it is still a terminal view rather than a graphical window.

Internally, the emulator now has a minimal Cortex-M0+ exception path for vector-table dispatch, stack frames, `SVC`, `BKPT`, `SysTick`, NVIC pending/enabled bits, and exception return; small SPI1/I2C1 FIFO models with busy/backpressure state; a minimal RP2040 DMA channel model that can move memory to MMIO such as SPI data registers; broad reset-done readback; and small XOSC/clock/PLL readiness stubs for SDK-style startup code. These are deliberately targeted models for the current PicoCalc firmware and staged vendor probes rather than a complete RP2040 implementation.

Focused emulator probe targets exercise newer model pieces:

```sh
make bin-emu-thumb-probe
make bin-emu-interrupt-probe
make bin-emu-dma-probe
```

The Thumb probe covers a previously missing Cortex-M0+ `LDRH` immediate decode. The interrupt probe exercises `SVC`, `SysTick`, exception return, and NVIC IRQ0. The DMA probe transfers a framebuffer block to SPI1 through the DMA MMIO path and records DMA trace events.

For a first PicoCalc-shaped host view around the emulator framebuffer, run:

```sh
make gui-hello
make gui-graphics
make gui-solve
```

These write `build/gui/bare_hello_picocalc.png`, `build/gui/bare_graphics_picocalc.png`, and `build/gui/bare_solve_picocalc.png`, procedurally drawing a minimalist PicoCalc body, bezel, d-pad, and keyboard around the 320x320 screen output.

The GUI shell currently displays the PicoCalc screen at 200% scale so text density and margins are easier to inspect on a desktop monitor.

The Cascadia Mono TTF in `vendor/microsoft/cascadia/CascadiaMono.ttf` can be converted into a compact firmware-ready bitmap font table with:

```sh
make font-cascadia
```

This uses the vendored NewOS fontrender copy in `vendor/newos/fontrender` on the host and writes `build/font/picocalc_cascadia_8x14.h` plus a preview image at `build/font/picocalc_cascadia_8x14.ppm`. The generated table stores 4-bit grayscale alpha values, two pixels per byte. The TTF renderer is not linked into PicoCalc firmware.

The bare and source-simulator LCD text renderers include that generated `8x14` alpha table and blend text pixels against the requested foreground/background colors. `make bare-graphics`, `make bare-solve`, and `make sim-graphics` generate the font header automatically when needed, so the PicoCalc firmware uses the compact alpha data only.

The host/ARM-probe Makefile builds define `PICOCALC_SOLVE_PROVIDE_MEMOPS` so the local shim supplies `memcpy`, `memset`, `memmove`, and `memcmp`. The Pico SDK build leaves those to the SDK/newlib runtime.