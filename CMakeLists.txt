cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "Set PICO_SDK_PATH to a local pico-sdk checkout before configuring this project")
endif()

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(picocalc_solve C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(vendor/PicoCalc/Code/picocalc_helloworld/i2ckbd)
add_subdirectory(vendor/PicoCalc/Code/pico_multi_booter/sd_boot/lcdspi)

set(PICOCALC_SOLVE_SOURCES
    src/solve.c
    src/support.c
    src/picocalc/sdk/platform_rp2040.c
    src/picocalc/sdk/picocalc_io.c
)

function(configure_picocalc_solve_target target)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Os
    )

    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
    )

    target_link_libraries(${target} PRIVATE
        pico_stdlib
        hardware_clocks
        hardware_i2c
        hardware_spi
        pico_multicore
        i2ckbd
        lcdspi
    )

    pico_enable_stdio_usb(${target} 1)
    pico_enable_stdio_uart(${target} 1)
    pico_add_extra_outputs(${target})
endfunction()

add_executable(picocalc_solve ${PICOCALC_SOLVE_SOURCES})
configure_picocalc_solve_target(picocalc_solve)

add_executable(picocalc_solve_sd ${PICOCALC_SOLVE_SOURCES})
configure_picocalc_solve_target(picocalc_solve_sd)
set_target_properties(picocalc_solve_sd PROPERTIES OUTPUT_NAME solve)

if(PICO_PLATFORM STREQUAL "rp2040")
    pico_set_linker_script(picocalc_solve_sd ${CMAKE_CURRENT_LIST_DIR}/vendor/PicoCalc/Code/pico_multi_booter/memmap_default_rp2040.ld)
elseif(PICO_PLATFORM MATCHES "rp2350")
    pico_set_linker_script(picocalc_solve_sd ${CMAKE_CURRENT_LIST_DIR}/vendor/PicoCalc/Code/pico_multi_booter/memmap_default_rp2350.ld)
endif()
