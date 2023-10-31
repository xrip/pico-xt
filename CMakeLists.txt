cmake_minimum_required(VERSION 3.22)
include(pico_sdk_import.cmake)

project(pico-xt C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(CMAKE_INCLUDE_PATH ${PROJECT_SOURCE_DIR}/include)
set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

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

    add_subdirectory(drivers/vga-nextgen)
    add_subdirectory(drivers/ps2)
endif ()

# INCLUDE FILES THAT SHOULD BE COMPILED:

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c" "src/*.h")


message(STATUS "Add source files:")
foreach (SRC_FILE IN LISTS SRC)
    message(STATUS "${SRC_FILE}")
endforeach ()
message(STATUS "")


add_executable(${PROJECT_NAME} ${SRC})

if (PICO_PLATFORM STREQUAL "host")
    target_link_libraries(${PROJECT_NAME} SDL2)
    message(STATUS "Copying all dependencies to destination folder...")
    file(GLOB PROJECT_DEPENDENCIES ${SDL2_LIBRARY}/*.dll)

    foreach (Dependency IN LISTS PROJECT_DEPENDENCIES)
        file(
                COPY ${Dependency}
                DESTINATION ${OUTPUT_DIR}
        )
    endforeach ()

else ()
    if(OVERCLOCKING)
        set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME  "${PROJECT_NAME}-${OVERCLOCKING}MHz")

        target_compile_definitions(${PROJECT_NAME} PRIVATE OVERCLOCKING=${OVERCLOCKING})

        pico_define_boot_stage2(slower_boot2 ${PICO_DEFAULT_BOOT_STAGE2_FILE})
        target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=4)
        pico_set_boot_stage2(${PROJECT_NAME} slower_boot2)

        message(STATUS "==================================")
        message(STATUS "Overclocking ${OVERCLOCKING}MHz enabled")
        message(STATUS "==================================")
        message(STATUS "")
    endif ()
    target_link_libraries(${PROJECT_NAME} PRIVATE
            vga-nextgen
            ps2

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
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
    PICO_DEFAULT_UART_RX_PIN=17
    PICO_DEFAULT_UART_TX_PIN=16
    )
endif ()

pico_enable_stdio_uart(${PROJECT_NAME} 1)
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_add_extra_outputs(${PROJECT_NAME})