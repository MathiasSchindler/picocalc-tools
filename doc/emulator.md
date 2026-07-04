# EMULATOR

## NAME

bin_emu - no-libc Linux emulator for PicoCalc/RP2040 flat `.bin` firmware images

## SYNOPSIS

```
build/emu/bin_emu INPUT.bin OUTPUT.png [KEYS]
build/emu/bin_emu INPUT.bin OUTPUT.gif [KEYS] --frames=N --gif-fps=N
build/emu/bin_emu INPUT.bin OUTPUT.png [KEYS] --trace[=PATH]
build/emu/bin_emu INPUT.bin OUTPUT.png [KEYS] --symbols=MAP

make bin-emu-cube
make bin-emu-cube-picolink
make bin-emu-cube-gif
make picolink-regression
```

## DESCRIPTION

`bin_emu` executes PicoCalc SD-app-style RP2040 firmware images on a Linux host. It loads a flat `.bin` image at the firmware application address, interprets the supported ARMv6-M Thumb instruction subset, models enough RP2040 and PicoCalc hardware for the local firmware targets, observes LCD SPI traffic, and writes a rendered framebuffer image.

The more accurate name is currently emulator. It executes guest machine instructions and models memory-mapped hardware behavior. Many peripheral models are intentionally approximate and test-driven, so some documents and conversations may still call it a simulator. In this repository, emulator means the executable firmware runner; simulator should be reserved for higher-level host-only builds that do not execute the RP2040 binary.

The emulator is a no-libc Linux host program. It uses `_start`, direct syscalls, and local image writers instead of libc or external PNG/GIF tools.

## CURRENT CAPABILITIES

- loads flat PicoCalc/RP2040 `.bin` images at the SD-app firmware address
- maps the firmware flash image, RAM, Boot2 stub reads, and targeted Boot ROM table/helper behavior
- interprets the ARMv6-M Thumb instructions used by current local firmware and staged vendor probes
- models enough reset, clock, timer, SIO, SysTick, NVIC, exception, DMA, SPI, I2C, UART, RTC, XIP SSI, LCD, and keyboard behavior for the active test cases
- decodes PicoCalc LCD command/data traffic into a 320x320 RGB framebuffer
- writes PNG output by default when the output path ends in `.png`
- writes PPM output only when the output path explicitly ends in `.ppm`
- writes looping GIF captures when the output path ends in `.gif` and frame capture is requested
- supports scripted keyboard input from an argument, stdin, or an `@file` replay
- supports trace output for base execution events, Boot ROM calls, unknown MMIO, and XIP SSI MMIO
- supports `pico_link` map files for symbol names in human-facing PC reports
- supports persistent flash-state files for Boot ROM flash erase/program experiments
- supports deterministic hash checks for regression tests

## INPUTS

`INPUT.bin` is a flat firmware image whose first words are an RP2040-style vector table. Local examples include:

- `build/bare/bare_cube.bin`, produced by the traditional GNU ld and objcopy path
- `build/bare/bare_cube_picolink.bin`, produced directly by `pico_link`
- `build/bare/bare_solve.bin`, `build/bare/bare_benchmark.bin`, and other SDK-free bare firmware targets

Vendor images under `vendor/images` are also used as compatibility probes. They are investigation inputs, not a promise of complete vendor firmware support.

## OPTIONS

- `KEYS` provide a literal keyboard script; C-style escapes such as `\n`, `\r`, `\t`, `\b`, `\\`, and `\xHH` are recognized by the current parser
- `-` read keyboard input from stdin
- `@PATH` read keyboard replay input from `PATH`
- `--frames=N` capture `N` frames instead of stopping after the first selected output point
- `--gif-fps=N` set GIF playback rate from `1` to `100` frames per second
- `--trace` write a default trace file beside the normal output path
- `--trace=PATH` write trace output to `PATH`
- `--trace-kinds=base,calls,unknown-mmio,xip|all` choose trace categories
- `--expect-hash=HEX` fail if the final output framebuffer hash differs from `HEX`
- `--max-steps=N` stop after at most `N` emulated instructions or accelerated-step equivalents
- `--fail-on-budget` return failure when the step budget is reached
- `--report-milestones` print first LCD command, first pixel, and first nonblack pixel milestones
- `--live-terminal` use live terminal keyboard input; if no key script is provided, stdin is used
- `--flash-state=PATH` load and save persistent emulated flash contents at `PATH`
- `--symbols=MAP` read a `pico_link` map file and annotate PCs in frame-ready, budget, crash, and LCD-milestone reports

## EXAMPLES

Run the traditional cube image and write a PNG:

```
make bin-emu-cube
```

Run the `pico_link` cube image and write a PNG:

```
make bin-emu-cube-picolink
```

Run the focused `pico_link` regression set, including the custom-linked cube, hybrid LTO cube, and solve images:

```
make picolink-regression
```

Capture the cube demo as an animated GIF:

```
make bin-emu-cube-gif
```

Run the emulator directly with a scripted solve input:

```
build/emu/bin_emu build/bare/bare_solve.bin build/emu/bare_solve.png '6x-3=0\n\x04'
```

Run with trace output and LCD milestone reporting:

```
build/emu/bin_emu build/bare/bare_vendor_startup_probe.bin \
  build/emu/bare_vendor_startup_probe.png \
  --trace=build/emu/bare_vendor_startup_probe.trace \
  --trace-kinds=base,calls,unknown-mmio \
  --report-milestones
```

## RELATION TO PICOLINK

`pico_link` produces flat `.bin` images. `bin_emu` is the fastest local way to check whether those images boot, run, and produce expected screen output before copying them to a PicoCalc.

The emulator does not require `pico_link` output to be byte-identical to GNU ld output. A `pico_link` image is acceptable when the vector table, memory layout, relocations, runtime behavior, and hardware-screen behavior are correct for the target profile.

When a `pico_link` map is available, pass `--symbols=MAP` to make budget and failure reports name the nearest symbol, for example `pc=0x1003304c (bare_main+0x00000527)`. The Makefile's picolink emulator targets pass their maps automatically.

## LIMITATIONS

- the emulator is not a complete RP2040 model
- the PicoCalc SD bootloader, UF2 flow, USB mass storage behavior, SD card filesystem, and firmware selection UI are not emulated
- peripheral models are targeted to local firmware and staged vendor probes rather than cycle-accurate hardware behavior
- many Boot ROM behaviors are focused stubs rather than the real RP2040 Boot ROM implementation
- timing is approximate; wait-loop acceleration and timer scaling are chosen for useful local behavior, not hardware-cycle accuracy
- vendor images may reach known step-budget frontiers rather than completing
- live terminal input is a host convenience, not a model of the PicoCalc keyboard controller firmware

## SEE ALSO

`doc/picolink.md`, `src/host/emulator/bin_emu.c`, `src/host/emulator/emu_mem.c`, `src/host/emulator/emu_bootrom.c`, `src/host/emulator/emu_lcd.c`, `src/host/linker/pico_link.c`, `Makefile`, `plan.md`