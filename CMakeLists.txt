cmake_minimum_required(VERSION 3.16)
project(gb-emulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# --- Emulator core (no SDL dependency) ---
add_library(gbcore
    src/gameboy.cpp
    src/bus.cpp
    src/cpu/cpu.cpp
    src/cpu/opcodes.cpp
    src/cpu/cb_opcodes.cpp
    src/mbc/rom_only.cpp
    src/mbc/mbc1.cpp
    src/mbc/mbc3.cpp
    src/mbc/mbc5.cpp
    src/ppu.cpp
    src/timer.cpp
    src/joypad.cpp
    src/apu.cpp
)
target_include_directories(gbcore PUBLIC include)

if(MSVC)
    target_compile_options(gbcore PRIVATE /W4)
else()
    target_compile_options(gbcore PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Executable ---
add_executable(gb-emulator src/main.cpp src/frontend/sdl_frontend.cpp)
target_link_libraries(gb-emulator PRIVATE gbcore)

# SDL2 is optional: without it, only the headless build is produced
find_package(SDL2 QUIET)
if(SDL2_FOUND)
    target_link_libraries(gb-emulator PRIVATE SDL2::SDL2)
    target_compile_definitions(gb-emulator PRIVATE GB_HAS_SDL)

    add_custom_command(TARGET gb-emulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL2::SDL2>"
        "$<TARGET_FILE_DIR:gb-emulator>"
)
else()
    message(STATUS "SDL2 not found - building without the frontend")
endif()

# --- Tests ---
enable_testing()

add_executable(cpu_tests tests/cpu_tests.cpp)
add_executable(ppu_tests tests/ppu_tests.cpp)
add_executable(mbc_tests tests/mbc_tests.cpp)
add_executable(apu_tests tests/apu_tests.cpp)
target_link_libraries(cpu_tests PRIVATE gbcore)
target_link_libraries(ppu_tests PRIVATE gbcore)
target_link_libraries(mbc_tests PRIVATE gbcore)
target_link_libraries(apu_tests PRIVATE gbcore)
add_test(NAME cpu_tests COMMAND cpu_tests)
add_test(NAME ppu_tests COMMAND ppu_tests)
add_test(NAME mbc_tests COMMAND mbc_tests)
add_test(NAME apu_tests COMMAND apu_tests)
