cmake_minimum_required(VERSION 3.22)
project(psp)

set(SDL_STATIC on)

add_subdirectory(vendors/spdlog)
add_subdirectory(vendors/CLI11)
add_subdirectory(vendors/SDL)
add_subdirectory(vendors/ELFIO)

set(CMAKE_CXX_STANDARD 20)

list(APPEND sources 
	src/main.cpp
	src/psp.cpp
	src/cpu.cpp

	src/renderer/renderer.cpp
	src/renderer/software/renderer.cpp

	src/kernel/memory.cpp
	src/kernel/kernel.cpp
	src/kernel/module.cpp
	src/kernel/thread.cpp
	src/kernel/callback.cpp

	src/hle/hle.cpp
	src/hle/modulemgrforuser.cpp
	src/hle/sysmemuserforuser.cpp
	src/hle/threadmanforuser.cpp
	src/hle/loadexecforuser.cpp
	src/hle/scedisplay.cpp
	src/hle/scege_user.cpp
	src/hle/sceutility.cpp
	src/hle/scenetinet.cpp
	src/hle/iofilemgrforuser.cpp
	src/hle/kernel_library.cpp
	src/hle/stdioforuser.cpp
)

add_executable(psp ${sources})
target_link_libraries(psp PRIVATE elfio::elfio spdlog::spdlog CLI11::CLI11 SDL2::SDL2-static SDL2::SDL2main)
target_include_directories(psp PRIVATE include)