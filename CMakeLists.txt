cmake_minimum_required(VERSION 3.22)
include(pico_sdk_import.cmake)

project(pico-xt C CXX ASM)
option(OVERCLOCKING "Target Pico MHz" OFF)
option(I2S_SOUND "Enable I2S sound" OFF)
if(NOT OVERCLOCKING)
    SET(OVERCLOCKING 408)
endif ()

SET(BUILD_NAME "pico-xt")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(CMAKE_INCLUDE_PATH ${PROJECT_SOURCE_DIR}/include)
set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -ffast-math -feliminate-unused-debug-types -ffunction-sections -fdata-sections -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -ffast-math -feliminate-unused-debug-types -ffunction-sections -fdata-sections -O2")

if (PICO_PLATFORM STREQUAL "host")
    set(SDL2_LIB_DIR C:/SDL/lib)
    set(SDL2_INCLUDE_DIR C:/SDL/include)
    set(SDL2_LIBRARY C:/SDL/bin)

    include_directories(
            ${SDL2_INCLUDE_DIR}
    )

    link_directories(
            ${SDL2_LIB_DIR}
    )

else ()
    #add_compile_definitions(${PROJECT_NAME} NO_GRAPHICS)
    set(FAMILY rp2040)
    set(BOARD pico_sdk)

    add_subdirectory(drivers/vga-nextgen)
    add_subdirectory(drivers/ps2)
    add_subdirectory(drivers/fatfs)
    add_subdirectory(drivers/sdcard)
    add_subdirectory(drivers/psram)
    add_subdirectory(drivers/nespad)
    add_subdirectory(drivers/audio)
endif ()


# INCLUDE FILES THAT SHOULD BE COMPILED:

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")


message(STATUS "Add source files:")
foreach (SRC_FILE IN LISTS SRC)
    message(STATUS "${SRC_FILE}")
endforeach ()
message(STATUS "")


add_executable(${PROJECT_NAME} ${SRC})

pico_set_program_name(pico-xt "PICO PC/XT 8086 Emulator by xrip/DnCraptor")
pico_set_program_version(pico-xt "test")

#set(PICO_DEOPTIMIZED_DEBUG 1)

if (PICO_PLATFORM STREQUAL "host")
    target_link_libraries(${PROJECT_NAME} SDL2)
    message(STATUS "Copying all dependencies to destination folder...")
    file(GLOB PROJECT_DEPENDENCIES ${SDL2_LIBRARY}/*.dll)

    foreach (Dependency IN LISTS PROJECT_DEPENDENCIES)
        file(COPY ${Dependency} DESTINATION ${OUTPUT_DIR})
    endforeach ()
    add_link_options(-Wl,--gc-sections)

else ()
    if (OVERCLOCKING)
        SET(BUILD_NAME "${BUILD_NAME}-${OVERCLOCKING}MHz")
        target_compile_definitions(${PROJECT_NAME} PRIVATE OVERCLOCKING=${OVERCLOCKING})

        pico_define_boot_stage2(slower_boot2 ${PICO_DEFAULT_BOOT_STAGE2_FILE})
        target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=4)
        pico_set_boot_stage2(${PROJECT_NAME} slower_boot2)

        message(STATUS "==================================")
        message(STATUS "Overclocking ${OVERCLOCKING}MHz enabled")
        message(STATUS "==================================")
        message(STATUS "")
    endif ()
    if (I2S_SOUND)
        SET(BUILD_NAME "${BUILD_NAME}-I2S")
        target_compile_definitions(${PROJECT_NAME} PRIVATE I2S_SOUND=1)

        message(STATUS "==================================")
        message(STATUS "I2S sound enabled")
        message(STATUS "==================================")
        message(STATUS "")
    endif()

    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${BUILD_NAME}")

    # Enable compiler and linker garbage collect unused code/data sections
    add_link_options(-Wl,--gc-sections)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            vga-nextgen
            ps2
            psram
            sdcard
            fatfs
            nespad
            audio

            pico_runtime
            pico_stdlib
            pico_rand
            pico_multicore
            hardware_dma
            hardware_pio
            hardware_i2c
            hardware_interp
            hardware_timer
            hardware_clocks
            hardware_pwm
            hardware_flash

            tinyusb_board
            tinyusb_device
    )
    target_link_options(${PROJECT_NAME} PRIVATE "LINKER:--script=${CMAKE_CURRENT_LIST_DIR}/memmap.ld")
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/memmap.ld)

    target_sources(${PROJECT_NAME} PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb/usb.c
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb/msc_disk.c
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb/usb_descriptors.c
    )
    target_include_directories(${PROJECT_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
    )
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${PROJECT_NAME} PUBLIC
                -mcpu=cortex-m0plus
                -Wno-error=suggest-attribute=format
                -Wno-error=cast-qual
                -Wno-error=unused-parameter
                -Wno-error=conversion
                -Wno-error=format=
                -Wno-error=sign-compare
                -Wno-error=missing-field-initializers
                -Wno-error=switch
                -Wno-error=implicit-fallthrough=
                -Wno-error=stringop-truncation
                -Wno-error=restrict
                -w
        )
    endif ()


    #[[    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND picotool info ${OUTPUT_DIR}/$<TARGET_PROPERTY:OUTPUT_NAME>.uf2 &&
                picotool reboot -f -u & mode COM8 baud=12 parity=n data=8 stop=1 & ping -n 3 127.1> nul &
                picotool load ${OUTPUT_DIR}/$<TARGET_PROPERTY:OUTPUT_NAME>.uf2 &&
                picotool reboot
        )]]

endif ()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        PICO_HEAP_SIZE=0
        USE_ZONE_FOR_MALLOC=1

        PICO_DEFAULT_UART_RX_PIN=-1
        PICO_DEFAULT_UART_TX_PIN=0

        # SDCARD
        SDCARD_PIN_SPI0_CS=5
        SDCARD_PIN_SPI0_SCK=2
        SDCARD_PIN_SPI0_MOSI=3
        SDCARD_PIN_SPI0_MISO=4

        # NES Gamepad
        USE_NESPAD
        NES_GPIO_CLK=14
        NES_GPIO_DATA=16
        NES_GPIO_LAT=15

        PSRAM
        # PSRAM_MUTEX=1
        PSRAM_SPINLOCK=1
        PSRAM_ASYNC=1

        PSRAM_PIN_CS=18
        PSRAM_PIN_SCK=19
        PSRAM_PIN_MOSI=20
        PSRAM_PIN_MISO=21

        # supports only for pico with 4 or more MB flashes
        PICO_FLASH_SIZE_BYTES=4194304

        #BOOT_DEBUG=1
        #BOOT_DEBUG_ACC=0xA0000

        # select onr of the folowing:
        #SWAP_BLOCK_1k
        SWAP_BLOCK_2k
        #SWAP_BLOCK_4k

        EMS_DRIVER=1
        #EMS_DEBUG=1

        XMS_DRIVER=1
        XMS_OVER_HMA_KB=1024
        MAX_XMM_HANDLES=60
        XMS_UMB=1
        XMS_HMA=1
        #XMS_DEBUG=1

        HANDLE_REBOOT

        SOUND_ENABLED
        #I2S_SOUND
        # PWM mode for i2s driver
        #AUDIO_PWM_PIN=26
        CMS
        #SOUND_BLASTER
        #ADLIB
        TANDY3V
        COVOX
        DSS

#        ROM_DRIVE_B=1

        #DMA_8237
)
target_link_options(${PROJECT_NAME} PRIVATE -Xlinker --print-memory-usage)

pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 0)

pico_add_extra_outputs(${PROJECT_NAME})
