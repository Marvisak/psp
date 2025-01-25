#include "kernel.hpp"

#include <spdlog/spdlog.h>

#include "../hle/defs.hpp"
#include "../psp.hpp"
#include "module.hpp"
#include "thread.hpp"

Kernel::Kernel() : 
	user_memory(USER_MEMORY_START, USER_MEMORY_END - USER_MEMORY_START, 0x100), 
	kernel_memory(KERNEL_MEMORY_START, USER_MEMORY_START - KERNEL_MEMORY_START, 0x100)
{


	user_memory.AllocAt(USER_MEMORY_START, 0x4000, "usersystemlib");
}

void Kernel::AllocateFakeSyscalls() {
	std::vector<uint32_t> opcodes;
	for (int i = 0; i < hle_imports.size(); i++) {
		auto& import_data = hle_imports[i];
		if (import_data.module == "FakeSyscalls") {
			opcodes.push_back((31 << 21) | 0x8);
			opcodes.push_back((i << 6) | 0xC);
		}
	}

	kernel_memory.AllocAt(KERNEL_MEMORY_START, sizeof(uint32_t) * opcodes.size(), "fakesyscalls");
	auto fake_syscalls_addr = PSP::GetInstance()->VirtualToPhysical(KERNEL_MEMORY_START);
	memcpy(fake_syscalls_addr, opcodes.data(), sizeof(uint32_t) * opcodes.size());
}

int Kernel::AddKernelObject(std::unique_ptr<KernelObject> obj) {
	for (int i = 0; i < objects.size(); i++) {
		if (!objects[i]) {
			obj->SetUID(i);
			objects[i] = std::move(obj);
			return i;
		}
	}
	return -1;
}

int Kernel::LoadModule(std::string path) {
	auto module = std::make_unique<Module>(path);
	if (!module->Load()) {
		spdlog::error("Kernel: failed loading module");
		return -1;
	}

	return AddKernelObject(std::move(module));
}

bool Kernel::ExecModule(int uid) {
	auto module = GetKernelObject<Module>(uid);
	if (!module) return false;
	
	auto thread = std::make_unique<Thread>(module, KERNEL_MEMORY_START);
	int thid = AddKernelObject(std::move(thread));
	StartThread(thid);

	return true;
}

int Kernel::Reschedule() {
	for (auto& ready_threads : thread_ready_queue) {
		if (ready_threads.empty())
			continue;

		int ready_thid = ready_threads.front();
		ready_threads.pop();

		auto ready_thread = GetKernelObject<Thread>(ready_thid);
		if (!ready_thread) {
			spdlog::error("Kernel: thread in ready queue doesn't exist");
			continue;
		}

		auto current_thread_obj = GetKernelObject<Thread>(current_thread);
		if (current_thread_obj)
			current_thread_obj->SwitchFrom();
		ready_thread->SwitchTo();
		current_thread = ready_thid;
		return ready_thid;
	}

	return current_thread;
}

int Kernel::CreateThread(std::string name, uint32_t entry, int init_priority, uint32_t stack_size, uint32_t attr) {
	if (stack_size < 0x200) {
		spdlog::error("Kernel: invalid stack size {:x}", stack_size);
		return SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE;
	}

	if (init_priority < 0x8 || init_priority > 0x77) {
		spdlog::error("Kernel: invalid init priority {:x}", init_priority);
		return SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
	}

	if (user_memory.GetLargestFreeBlock() < stack_size) {
		spdlog::error("Kernel: not enough space for thread {:x}", stack_size);
		return SCE_KERNEL_ERROR_NO_MEMORY;
	}

	if (PSP::GetInstance()->VirtualToPhysical(entry) == 0) {
		spdlog::error("Kernel: invalid entry {:x}", entry);
		return SCE_KERNEL_ERROR_ILLEGAL_ENTRY;
	}

	auto thread = std::make_unique<Thread>(name, entry, init_priority, stack_size, KERNEL_MEMORY_START + 8);
	return AddKernelObject(std::move(thread));
}

void Kernel::DeleteThread(int thid) {
	if (current_thread == thid) {
		if (Reschedule() == thid) {
			current_thread = -1;
		}
	}
	objects[thid] = nullptr;
}

int Kernel::StartThread(int thid) {
	auto thread = GetKernelObject<Thread>(thid);
	if (!thread) return SCE_KERNEL_ERROR_UNKNOWN_THID;
	if (thread->GetState() != ThreadState::DORMANT) return SCE_KERNEL_ERROR_NOT_DORMANT;

	thread->SetState(ThreadState::READY);
	thread_ready_queue[thread->GetPriority()].push(thid);

	return SCE_KERNEL_ERROR_OK;
}

void Kernel::ExecHLEFunction(int import_index) {
	auto& import_data = hle_imports[import_index];
	auto& func = hle_modules[import_data.module][import_data.nid];
	func(PSP::GetInstance()->GetCPU());
}
