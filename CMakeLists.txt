cmake_minimum_required(VERSION 3.22)
project(psp)

set(SDL_STATIC on)
set(SOCKPP_BUILD_STATIC on)

add_subdirectory(vendors/spdlog)
add_subdirectory(vendors/CLI11)
add_subdirectory(vendors/SDL)
add_subdirectory(vendors/ELFIO)
add_subdirectory(vendors/sockpp)
add_subdirectory(vendors/glm)

set(CMAKE_CXX_STANDARD 23)

list(APPEND sources 
	src/main.cpp
	src/psp.cpp
	src/cpu.cpp

	src/debugger/debugger.cpp

	src/renderer/renderer.cpp
	src/renderer/software/renderer.cpp

	src/kernel/memory.cpp
	src/kernel/kernel.cpp
	src/kernel/module.cpp
	src/kernel/thread.cpp
	src/kernel/callback.cpp
	src/kernel/memory_block.cpp
	src/kernel/semaphore.cpp

	src/kernel/filesystem/directory/directoryfs.cpp
	src/kernel/filesystem/directory/directoryfile.cpp

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
	src/hle/scectrl.cpp
	src/hle/scedmac.cpp
	src/hle/utilsforuser.cpp
	src/hle/sceaudio.cpp
	src/hle/scepower.cpp
	src/hle/sceatrac3plus.cpp
	src/hle/sceumduser.cpp
	src/hle/scertc.cpp
)

add_executable(psp ${sources})
target_link_libraries(psp PRIVATE Sockpp::sockpp-static elfio::elfio spdlog::spdlog CLI11::CLI11 SDL3::SDL3-static glm::glm)
target_include_directories(psp PRIVATE include)
