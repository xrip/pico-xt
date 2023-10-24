cmake_minimum_required(VERSION 3.22)

project(tiny8086 C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_INCLUDE_PATH ${PROJECT_SOURCE_DIR}/include)
set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

set(SDL2_LIB_DIR C:/SDL/lib)
set(SDL2_INCLUDE_DIR C:/SDL/include)
set(SDL2_LIBRARY C:/SDL/bin)

add_compile_definitions(${PROJECT_NAME} FAKE86_EMULATOR)

#add_subdirectory(megadrive)

include_directories(
        ${SDL2_INCLUDE_DIR}
)

link_directories(
        ${SDL2_LIB_DIR}
)

# INCLUDE FILES THAT SHOULD BE COMPILED:

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c" "src/*.h")


message(STATUS "Add source files:")
foreach(SRC_FILE IN LISTS SRC)
    message(STATUS "${SRC_FILE}")
endforeach()
message(STATUS "")


add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} SDL2)

message(STATUS "Copying all dependencies to destination folder...")
file(GLOB PROJECT_DEPENDENCIES ${SDL2_LIBRARY}/*.dll)

foreach(Dependency IN LISTS PROJECT_DEPENDENCIES)
    file(
            COPY ${Dependency}
            DESTINATION ${OUTPUT_DIR}
    )
endforeach()