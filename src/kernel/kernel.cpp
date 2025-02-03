#include "kernel.hpp"

#include <spdlog/spdlog.h>

#include "../hle/defs.hpp"
#include "../psp.hpp"
#include "module.hpp"
#include "thread.hpp"
#include "callback.hpp"

Kernel::Kernel() {
	user_memory = std::make_unique<MemoryAllocator>(USER_MEMORY_START, USER_MEMORY_END - USER_MEMORY_START, 0x100);
	kernel_memory = std::make_unique<MemoryAllocator>(KERNEL_MEMORY_START, USER_MEMORY_START - KERNEL_MEMORY_START, 0x100);
	user_memory->AllocAt(USER_MEMORY_START, 0x4000, "usersystemlib");

	std::vector<uint32_t> opcodes;
	for (int i = 0; i < hle_imports.size(); i++) {
		auto& import_data = hle_imports[i];
		if (import_data.module == "FakeSyscalls") {
			opcodes.push_back((31 << 21) | 0x8);
			opcodes.push_back((i << 6) | 0xC);
		}
	}

	kernel_memory->AllocAt(KERNEL_MEMORY_START, sizeof(uint32_t) * opcodes.size(), "fakesyscalls");
	auto fake_syscalls_addr = PSP::GetInstance()->VirtualToPhysical(KERNEL_MEMORY_START);
	memcpy(fake_syscalls_addr, opcodes.data(), sizeof(uint32_t) * opcodes.size());
}

int Kernel::AddKernelObject(std::unique_ptr<KernelObject> obj) {
	for (int i = next_uid; i < objects.size(); i++) {
		if (!objects[i]) {
			obj->SetUID(i);
			objects[i] = std::move(obj);
			++next_uid %= objects.size();
			return i;
		}
	}
	spdlog::error("Kernel: unable to assing uid to object");
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
	exec_module = uid;

	return true;
}

// TODO: Maybe add some cycle counting to this?
int Kernel::Reschedule() {
	auto current_thread_obj = GetKernelObject<Thread>(current_thread);
	bool current_thread_valid = current_thread_obj && current_thread_obj->GetState() == ThreadState::READY;
	for (int priority = 0; priority < thread_ready_queue.size(); priority++) {
		auto& ready_threads = thread_ready_queue[priority];
		if (ready_threads.empty())
			continue;

		if (current_thread_valid && current_thread_obj->GetPriority() >= priority)
			break;

		int ready_thid = ready_threads.front();
		ready_threads.pop();

		auto ready_thread = GetKernelObject<Thread>(ready_thid);
		if (!ready_thread) {
			spdlog::error("Kernel: thread in ready queue doesn't exist");
			continue;
		}

		if (current_thread_obj)
			current_thread_obj->SaveState();
		ready_thread->SwitchState();
		current_thread = ready_thid;
		return ready_thid;
	}

	if (!current_thread_valid) {
		if (current_thread_obj) current_thread_obj->SaveState();
		current_thread = -1;
	}

	return current_thread;
}

int Kernel::CreateThread(std::string name, uint32_t entry, int init_priority, uint32_t stack_size, uint32_t attr) {
	if (stack_size < 0x200) {
		spdlog::warn("Kernel: invalid stack size {:x}", stack_size);
		return SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE;
	}

	if (init_priority < 0x8 || init_priority > 0x77) {
		spdlog::warn("Kernel: invalid init priority {:x}", init_priority);
		return SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
	}

	if (user_memory->GetLargestFreeBlockSize() < stack_size) {
		spdlog::warn("Kernel: not enough space for thread {:x}", stack_size);
		return SCE_KERNEL_ERROR_NO_MEMORY;
	}

	if (PSP::GetInstance()->VirtualToPhysical(entry) == 0) {
		spdlog::warn("Kernel: invalid entry {:x}", entry);
		return SCE_KERNEL_ERROR_ILLEGAL_ENTRY;
	}

	PSP::GetInstance()->EatCycles(32000);
	auto thread = std::make_unique<Thread>(name, entry, init_priority, stack_size, KERNEL_MEMORY_START + 8);
	return AddKernelObject(std::move(thread));
}

void Kernel::DeleteThread(int thid) {
	if (current_thread == thid) {
		current_thread = -1;
		if (Reschedule() == thid) {
			spdlog::warn("Kernel: no thread to reschedule to");
		}
	}
	objects[thid] = nullptr;
}

void Kernel::StartThread(int thid) {
	auto thread = GetKernelObject<Thread>(thid);

	thread->SetState(ThreadState::READY);
	thread_ready_queue[thread->GetPriority()].push(thid);
	Reschedule();
}

int Kernel::CreateCallback(std::string name, uint32_t entry, uint32_t common) {
	if (!PSP::GetInstance()->VirtualToPhysical(entry)) {
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	auto callback = std::make_unique<Callback>(GetCurrentThread(), name, entry, common);
	int cbid = AddKernelObject(std::move(callback));
	return cbid;
}

void Kernel::ExecuteCallback(int cbid) {
	auto callback = GetKernelObject<Callback>(cbid);
	if (!callback) {
		spdlog::warn("Kernel: attempted to execute invalid callback {}", cbid);
		return;
	}

	current_callback = cbid;
	callback->Execute();
}

void Kernel::WakeUpThread(int thid, WaitReason reason) {
	Thread* thread = GetKernelObject<Thread>(thid);
	if (thread->GetState() != ThreadState::WAIT || thread->GetWaitReason() != reason) { return; }
	thread->SetState(ThreadState::READY);
	thread->SetWaitReason(WaitReason::NONE);

	thread_ready_queue[thread->GetPriority()].push(thid);
	Reschedule();
}

void Kernel::WaitCurrentThread(WaitReason reason, bool allow_callbacks) {
	Thread* thread = GetKernelObject<Thread>(current_thread);
	thread->SetState(ThreadState::WAIT);
	thread->SetWaitReason(reason);
	thread->SetAllowCallbacks(allow_callbacks);
	Reschedule();
}

void Kernel::WakeUpVBlank() {
	for (auto& object : objects) {
		if (object && object->GetType() == KernelObjectType::THREAD) {
			auto thread = reinterpret_cast<Thread*>(object.get());
			WakeUpThread(thread->GetUID(), WaitReason::VBLANK);
		}
	}
}

void Kernel::ExecHLEFunction(int import_index) {
	auto& import_data = hle_imports[import_index];
	auto& func = hle_modules[import_data.module][import_data.nid];

	auto cpu = PSP::GetInstance()->GetCPU();
	func(cpu);
}
