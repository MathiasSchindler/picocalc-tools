# PICOLINK

## NAME

picolink - PicoCalc-profile no-libc linker from ARM object files to flat `.bin` images

## SYNOPSIS

```
pico_link [--stats] [--map=MAP] [--no-gc-sections] [--order=reach] -o OUTPUT.bin INPUT.o ... [ARCHIVE.a ...]

make bare-cube-picolink
```

## DESCRIPTION

`pico_link` links a small, fixed bare-metal PicoCalc/RP2040 profile. It reads ELF32 little-endian ARM relocatable `.o` files and writes a flat SD-app-style `.bin` image linked for the PicoCalc application address.

It is not intended to be a general-purpose ELF linker. It exists so this repository can build PicoCalc firmware without depending on `arm-none-eabi-ld` and `arm-none-eabi-objcopy` for the final link-and-flatten stage.

The current implementation is a no-libc Linux host program. It uses `_start`, direct Linux syscalls, static storage, and the shared `host_nolibc.h` helpers.

## DESIGN GOALS

- produce valid PicoCalc SD-app `.bin` files directly from ARM `.o` inputs
- keep the supported profile narrow enough to inspect and improve confidently
- avoid byte-identical compatibility with GNU ld when that would block smaller, faster, or simpler output
- prefer measurable firmware behavior over layout fidelity as the correctness test
- leave room for optional layout, helper, and code-size optimizations that improve the generated image by practical metrics

## CURRENT CAPABILITIES

`pico_link` currently supports the relocation and section surface used by the cube demo path:

- ELF32 little-endian ARM relocatable input files
- allocatable `PROGBITS` and `NOBITS` sections
- `.vectors` first in the output image
- executable `.text*` sections in flash
- read-only `.rodata*` sections in flash
- writable `.data*` sections with RAM runtime addresses and flash load bytes
- `.bss*` sections with RAM addresses and no emitted bytes
- linker-provided symbols for startup code, including `__data_source`, `__data_start`, `__data_end`, `__bss_start`, `__bss_end`, `__text_start`, `__text_end`, `__StackTop`, and `__stack`
- `R_ARM_ABS32` relocations
- `R_ARM_REL32` relocations
- `R_ARM_THM_CALL` relocations
- weak symbol fallback to zero for unresolved weak references
- profile-specific section reachability garbage collection rooted at `.vectors`
- text map output for section and symbol inspection
- compact link statistics for output size, BSS size, relocation counts, and kept/discarded sections
- optional reachability-ordered section layout for call-graph locality experiments
- selective Unix `ar` archive input for small local runtime archives
- GCC LTO handoff through `-flinker-output=nolto-rel` native relocatable objects
- GCC relocatable runtime prelink handoff for larger firmware objects before final flattening
- synthetic PicoCalc profile symbols for startup code: `__data_source`, `__data_start`, `__data_end`, `__bss_start`, `__bss_end`, `__StackTop`, and `__stack`
- direct flat binary output

The `bare-cube-picolink` and `bare-solve-picolink` targets also link local ARM EABI helper code through `build/bare/libpico_runtime.a`, instead of asking `pico_link` to read all of `libgcc.a`. The archive contains 32-bit division helpers, 64-bit integer multiply/divide/shift helpers, the Thumb-1 byte switch helper used by GCC, and local double conversion, comparison, addition, subtraction, multiplication, and division helpers. The 32-bit unsigned division core uses a normalized shift/subtract loop with early exits, and quotient-only calls have their own entry points so plain `/` does not need to compute the remainder path used by `/` plus `%`.

## MEMORY PROFILE

The built-in memory profile matches `src/picocalc/bare/memmap_sd_rp2040.ld` for the SD-app firmware shape:

| Region | Address | Use |
| --- | ---: | --- |
| app flash | `0x10032000` | start of emitted `.bin` image |
| RAM | `0x20000000` | runtime `.data` and `.bss` addresses |
| stack top | `0x20042000` | first vector-table word |

The output file starts at `0x10032000`; addresses before that are not represented in the flat file.

`pico_link` provides the startup symbols that GNU ld normally gets from `memmap_sd_rp2040.ld`. `__data_source` is the flash load address for copied `.data`; `__data_start` and `__data_end` bound the RAM `.data` region; `__bss_start` and `__bss_end` bound the zeroed RAM `.bss` region; `__StackTop` and `__stack` resolve to `0x20042000`. These symbols are also written to map files so emulator reports and regression checks can inspect them.

## OPTIONS

- `-o OUTPUT.bin` write the linked flat binary to `OUTPUT.bin`
- `--stats` print section, relocation, image-size, and BSS-size statistics after linking
- `-v` alias for `--stats`
- `--map=MAP` write a text map file to `MAP`
- `--map MAP` write a text map file to `MAP`
- `--no-gc-sections` keep every allocatable input section instead of applying profile-specific section reachability
- `--order=reach` lay out sections within each output class by reachability order instead of input order
- `--order=none` force the default compact input-order layout
- `INPUT.o` read one or more ELF32 ARM relocatable object files in the order given
- `ARCHIVE.a` scan a Unix `ar` archive and extract members that define currently unresolved global symbols

There are no linker-script, library-search, final-ELF, or debug-metadata options yet.

## EXAMPLES

Build the current custom-linked cube image:

```
make bare-cube-picolink
```

Run the custom-linked cube image in the emulator:

```
make bin-emu-cube-picolink
```

Compare the GNU-linked and `pico_link` cube images by size and first-frame emulator timing:

```
make cube-link-compare
```

Build and run the current GNU LTO solve firmware. The solve target keeps `support.o` non-LTO so the freestanding `memcpy`/`memset` helpers remain ordinary Thumb code:

```
make bare-solve-lto
make bin-emu-solve-lto
```

The resulting `build/bare-lto/bare_solve_lto.bin` has been validated in the emulator and on a physical PicoCalc.

Build and run the current `pico_link` solve firmware. This path uses GCC for a relocatable prelink, while `pico_link` still performs the final PicoCalc memory layout and flat `.bin` emission:

```
make bare-solve-picolink
make bin-emu-solve-picolink
```

The resulting `build/bare/bare_solve_picolink.bin` matches the normal solve framebuffer hash in the emulator.

Build and run a hybrid LTO cube image. GCC first compiles the LTO program into a normal relocatable object with `-flinker-output=nolto-rel`; `pico_link` then maps that object to the final flat `.bin`:

```
make bare-cube-lto-picolink
make bin-emu-cube-lto-picolink
```

The resulting `build/bare-lto/bare_cube_lto_picolink.bin` has been validated in the emulator and on a physical PicoCalc.

Run the focused `pico_link` regression checks:

```
make picolink-regression
```

This builds the current custom-linked cube, hybrid LTO cube, and solve images, runs emulator hash checks, uses map-aware PC reporting, and prints image-size comparisons.

The regression also checks each `pico_link` map with `tests/check_picolink_symbols.awk`, ensuring the synthetic startup symbols are present, unique, inside the PicoCalc profile ranges, and consistent with the emitted image size.

Run the local double-helper probe:

```
make bin-emu-aeabi-double-probe
```

This verifies local double conversion, comparison, and arithmetic helpers against compile-time bit-pattern expectations, edge cases around signed zero, NaN, infinity, subnormals, integer saturation, arithmetic overflow, division by zero, and rounding around `2^53`.

Call `pico_link` directly:

```
build/linker/pico_link -o build/bare/bare_cube_picolink.bin \
  build/bare/start.o \
  build/bare/picocalc_lcd_bare.o \
  build/bare/picocalc_kbd_bare.o \
  build/bare/cube.o \
  build/bare/libpico_runtime.a
```

Write a map file and print link statistics:

```
build/linker/pico_link --stats --map=build/bare/bare_cube_picolink.map \
  -o build/bare/bare_cube_picolink.bin \
  build/bare/start.o \
  build/bare/picocalc_lcd_bare.o \
  build/bare/picocalc_kbd_bare.o \
  build/bare/cube.o \
  build/bare/libpico_runtime.a
```

## OUTPUT DIFFERENCES

`pico_link` does not try to reproduce GNU ld and `objcopy` byte-for-byte. The initial vector table and runtime ABI must be correct, but section order, helper implementations, addresses of later functions, file size, timing, and framebuffer hashes may differ.

This is intentional. The custom linker is allowed to become more aggressive than the traditional path when that makes the firmware smaller, faster, easier to inspect, or easier for the emulator and hardware tests to validate.

The current `bare_cube_picolink.bin` is smaller than `bare_cube.bin` because it uses local division helpers and a simpler built-in layout instead of the GNU linker plus libgcc layout.

## LTO NOTES

`pico_link` does not load GCC's LTO plugin and does not parse `.gnu.lto*` compiler IR sections directly. The supported LTO path is a handoff: invoke `arm-none-eabi-gcc -r -flto=auto -flinker-output=nolto-rel ...` to let GCC perform LTO and emit an ordinary ARM relocatable object, then pass that object to `pico_link`.

This keeps `pico_link` dependency-free while still allowing GCC to perform cross-translation-unit optimization before the final PicoCalc-profile link-and-flatten step.

Full GNU LTO also works for the solve firmware when the freestanding memory-helper object is compiled without LTO; `bare_solve_lto.bin` has been validated in both the emulator and on hardware. A full GNU LTO cube image builds and produces a smaller binary, but the current emulator does not complete its first-frame capture within the normal cube budget; the hybrid LTO object linked through `pico_link` does complete, matches the GNU cube framebuffer hash in the emulator, and runs on hardware.

The non-LTO `bare-solve-picolink` target currently uses a GCC `-r` handoff to merge the firmware objects before the final `pico_link` pass. Integer division, 64-bit integer arithmetic, Thumb switch helpers, and double conversion/comparison/arithmetic helpers are supplied by the local PicoCalc runtime archive. That is a pragmatic replacement for GNU's final link step, not a general-purpose linker yet. The next runtime-size step is to continue trimming helper implementations against PicoCalc workloads and to reduce solve's output path where normal C semantics allow it.

## LIMITATIONS

- only the PicoCalc SD-app memory profile is implemented
- only ELF32 little-endian ARM relocatable inputs are accepted
- only `R_ARM_ABS32`, `R_ARM_REL32`, and `R_ARM_THM_CALL` relocations are supported
- archive support is intentionally narrow: it reads ordinary Unix `ar` members by simple names and extracts only members satisfying unresolved globals
- library search paths are not implemented, so archive paths must be passed explicitly
- no linker script parser is implemented
- no final ELF, full symbol table dump, or debug metadata is emitted
- raw `.gnu.lto*` objects are not consumed directly; use GCC's `nolto-rel` handoff for LTO input
- garbage collection is profile-specific and simpler than GNU `--gc-sections`
- relocation overflow checks are minimal outside Thumb call range validation
- the host executable currently targets Linux x86-64 no-libc syscalls

## SEE ALSO

`doc/emulator.md`, `src/host/linker/pico_link.c`, `src/picocalc/bare/aeabi_div.S`, `src/picocalc/bare/memmap_sd_rp2040.ld`, `Makefile`