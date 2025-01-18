cmake_minimum_required(VERSION 3.22)
project(psp)

set(SDL_STATIC on)

add_subdirectory(vendors/spdlog)
add_subdirectory(vendors/CLI11)
add_subdirectory(vendors/SDL)
add_subdirectory(vendors/ELFIO)

set(CMAKE_CXX_STANDARD 20)

list(APPEND sources src/main.cpp src/psp.cpp src/hw/cpu.cpp src/kernel/memory.cpp src/kernel/kernel.cpp src/kernel/module.cpp src/kernel/thread.cpp)

add_executable(psp ${sources})
target_link_libraries(psp PRIVATE elfio::elfio spdlog::spdlog CLI11::CLI11 SDL2::SDL2-static SDL2::SDL2main)
target_include_directories(psp PRIVATE include)