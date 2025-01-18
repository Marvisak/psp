#pragma once

#include <cstdint>
#include <array>

// At this point this is incredibly simple, will be expanded (with actual context switching)
// once there are actually some user created threads

constexpr auto STACK_SIZE = 0x40000; // Change this probably;

class Thread {
public:
	Thread(uint32_t entrypoint, uint32_t stack_addr);
	
	void SetSavedRegister(int index, uint32_t value);
	void SwitchTo();
private:
	std::array<uint32_t, 31> saved_regs;

	uint32_t stack_addr;
	uint32_t pc;
};