cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(tomogatchi C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# ── FatFs (SD card filesystem for USB MSC) ────────────────────────────────────
include(FetchContent)
FetchContent_Declare(fatfs
    URL https://github.com/abbrev/fatfs/archive/refs/tags/R0.15.zip
)
FetchContent_MakeAvailable(fatfs)

# Overwrite bundled ffconf.h with ours before anything compiles
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ffconf.h
    ${fatfs_SOURCE_DIR}/source/ffconf.h
    COPYONLY
)

add_library(fatfs_lib STATIC
    ${fatfs_SOURCE_DIR}/source/ff.c
    ${fatfs_SOURCE_DIR}/source/ffsystem.c
    ${fatfs_SOURCE_DIR}/source/ffunicode.c
)
target_include_directories(fatfs_lib PUBLIC
    ${fatfs_SOURCE_DIR}/source
)

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(tomogatchi
    src/main.c
    src/st7735.c
    src/bmp.c
    src/sd_card.c
    src/usb_msc.c
)

target_include_directories(tomogatchi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${fatfs_SOURCE_DIR}/source
)

target_link_libraries(tomogatchi
    pico_stdlib
    hardware_spi
    hardware_gpio
    hardware_timer
    tinyusb_device
    tinyusb_board
    fatfs_lib
)

# UART for debug output; USB port is used exclusively for MSC
pico_enable_stdio_usb(tomogatchi 0)
pico_enable_stdio_uart(tomogatchi 1)

pico_add_extra_outputs(tomogatchi)