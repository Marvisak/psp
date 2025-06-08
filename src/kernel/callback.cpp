#include "callback.hpp"

#include <spdlog/spdlog.h>

Callback::Callback(int thid, std::string name, uint32_t entry, uint32_t common)
	: thid(thid), name(name), entry(entry), common(common) {}

void Callback::Execute(std::shared_ptr<WaitObject> wait) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	auto cpu = psp->GetCPU();

	ReturnData current_return_data {};
	current_return_data.v0 = cpu->GetRegister(MIPS_REG_V0);
	current_return_data.v1 = cpu->GetRegister(MIPS_REG_V1);
	current_return_data.addr = cpu->GetPC();
	current_return_data.wait = wait;
	return_data.push_front(current_return_data);

	cpu->SetPC(entry);
	cpu->SetRegister(MIPS_REG_A0, notify_count);
	cpu->SetRegister(MIPS_REG_A1, notify_arg);
	cpu->SetRegister(MIPS_REG_A2, common);
	cpu->SetRegister(MIPS_REG_RA, KERNEL_MEMORY_START + 0x10);

	notify_count = 0;
	notify_arg = 0;
}

std::shared_ptr<WaitObject> Callback::Return() {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	auto cpu = psp->GetCPU();

	auto& current_return_data = return_data.front();
	cpu->SetRegister(MIPS_REG_V0, current_return_data.v0);
	cpu->SetRegister(MIPS_REG_V1, current_return_data.v1);
	cpu->SetPC(current_return_data.addr);
	return_data.pop_front();

	return current_return_data.wait;
}

void Callback::Notify(int arg) {
	notify_count++;
	notify_arg = arg;
}

void Callback::Cancel() {
	notify_count = 0;
	notify_arg = 0;
}