# PICOLINK

## NAME

picolink - PicoCalc-profile no-libc linker from ARM object files to flat `.bin` images

## SYNOPSIS

```
pico_link -o OUTPUT.bin INPUT.o ...

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
- `R_ARM_THM_CALL` relocations
- weak symbol fallback to zero for unresolved weak references
- direct flat binary output

The `bare-cube-picolink` target also links `src/picocalc/bare/aeabi_div.S`, a small local ARM EABI division-helper object, instead of asking `pico_link` to read `libgcc` archives.

## MEMORY PROFILE

The built-in memory profile matches `src/picocalc/bare/memmap_sd_rp2040.ld` for the SD-app firmware shape:

| Region | Address | Use |
| --- | ---: | --- |
| app flash | `0x10032000` | start of emitted `.bin` image |
| RAM | `0x20000000` | runtime `.data` and `.bss` addresses |
| stack top | `0x20042000` | first vector-table word |

The output file starts at `0x10032000`; addresses before that are not represented in the flat file.

## OPTIONS

- `-o OUTPUT.bin` write the linked flat binary to `OUTPUT.bin`
- `INPUT.o` read one or more ELF32 ARM relocatable object files in the order given

There are no linker-script, library-search, archive, map-file, or debug-output options yet.

## EXAMPLES

Build the current custom-linked cube image:

```
make bare-cube-picolink
```

Run the custom-linked cube image in the emulator:

```
make bin-emu-cube-picolink
```

Call `pico_link` directly:

```
build/linker/pico_link -o build/bare/bare_cube_picolink.bin \
  build/bare/start.o \
  build/bare/picocalc_lcd_bare.o \
  build/bare/picocalc_kbd_bare.o \
  build/bare/cube.o \
  build/bare/aeabi_div.o
```

## OUTPUT DIFFERENCES

`pico_link` does not try to reproduce GNU ld and `objcopy` byte-for-byte. The initial vector table and runtime ABI must be correct, but section order, helper implementations, addresses of later functions, file size, timing, and framebuffer hashes may differ.

This is intentional. The custom linker is allowed to become more aggressive than the traditional path when that makes the firmware smaller, faster, easier to inspect, or easier for the emulator and hardware tests to validate.

The current `bare_cube_picolink.bin` is smaller than `bare_cube.bin` because it uses local division helpers and a simpler built-in layout instead of the GNU linker plus libgcc layout.

## LIMITATIONS

- only the PicoCalc SD-app memory profile is implemented
- only ELF32 little-endian ARM relocatable inputs are accepted
- only `R_ARM_ABS32` and `R_ARM_THM_CALL` relocations are supported
- no archive reader is implemented, so `.a` libraries such as `libgcc.a` are not searched
- no linker script parser is implemented
- no final ELF, map file, symbol table dump, or debug metadata is emitted
- garbage collection is profile-specific and currently much simpler than GNU `--gc-sections`
- relocation overflow checks are minimal outside Thumb call range validation
- the host executable currently targets Linux x86-64 no-libc syscalls

## SEE ALSO

`doc/emulator.md`, `src/host/linker/pico_link.c`, `src/picocalc/bare/aeabi_div.S`, `src/picocalc/bare/memmap_sd_rp2040.ld`, `Makefile`