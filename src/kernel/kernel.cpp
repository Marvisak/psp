#include "kernel.hpp"

#include <spdlog/spdlog.h>

#include "../hle/defs.hpp"
#include "../psp.hpp"
#include "module.hpp"
#include "thread.hpp"
#include "callback.hpp"
#include "filesystem/filesystem.hpp"

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

Kernel::~Kernel() {}

int Kernel::AddKernelObject(std::unique_ptr<KernelObject> obj) {
	for (int i = next_uid; i < objects.size(); i++) {
		if (!objects[i]) {
			obj->SetUID(i);
			objects[i] = std::move(obj);
			++next_uid %= objects.size();
			if (next_uid == 0) next_uid++;
			return i;
		}
	}
	spdlog::error("Kernel: unable to assing uid to object");
	return 0;
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
	reschedule_next_cycle = false;
	for (int priority = 0; priority < thread_ready_queue.size(); priority++) {
		auto& ready_threads = thread_ready_queue[priority];
		if (ready_threads.empty())
			continue;

		if (current_thread_valid && current_thread_obj->GetPriority() <= priority)
			break;

		Thread* ready_thread = nullptr;
		for (auto thid : ready_threads) {
			auto thread = GetKernelObject<Thread>(thid);
			if (!thread) {
				spdlog::error("Kernel: thread in ready queue doesn't exist");
				continue;
			}
			if (thread->GetState() == ThreadState::READY) {
				ready_thread = thread;
			}
		}

		if (!ready_thread) {
			continue;
		}

		if (current_thread_valid) {
			thread_ready_queue[current_thread_obj->GetPriority()].push_back(current_thread);
		}

		if (current_thread_obj) {
			current_thread_obj->SaveState();
		}

		ready_thread->SwitchState();
		current_thread = ready_thread->GetUID();
		return current_thread;
	}

	if (!current_thread_valid) {
		if (current_thread_obj) current_thread_obj->SaveState();
		current_thread = -1;
	}

	return current_thread;
}

int Kernel::CreateThread(std::string name, uint32_t entry, int init_priority, uint32_t stack_size, uint32_t attr) {
	PSP::GetInstance()->EatCycles(32000);
	auto thread = std::make_unique<Thread>(name, entry, init_priority, stack_size, attr, KERNEL_MEMORY_START + 8);
	return AddKernelObject(std::move(thread));
}

int Kernel::TerminateThread(int thid, bool del) {
	auto thread = GetKernelObject<Thread>(thid);
	if (!thread) {
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	auto& queue = thread_ready_queue[thread->GetPriority()];
	queue.erase(std::remove(queue.begin(), queue.end(), current_thread), queue.end());
	RescheduleNextCycle();

	if (del) {
		RemoveKernelObject(thid);
	} else {
		if (thread->GetState() == ThreadState::DORMANT) {
			return SCE_KERNEL_ERROR_DORMANT;
		}

		thread->SetState(ThreadState::DORMANT);
	}

	return 0;
}

void Kernel::StartThread(int thid) {
	auto thread = GetKernelObject<Thread>(thid);

	thread->Start();
	thread->SetState(ThreadState::READY);
	thread_ready_queue[thread->GetPriority()].push_back(thid);
	RescheduleNextCycle();
}

int Kernel::CreateCallback(std::string name, uint32_t entry, uint32_t common) {
	if (entry & 0xF0000000) {
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

void Kernel::Mount(std::string mount_point, std::shared_ptr<FileSystem> file_system) {
	drives[mount_point] = file_system;
}

int Kernel::OpenFile(std::string path, int flags) {
	int drive_end = path.find('/');
	std::string relative_path = path.substr(drive_end + 1);
	std::string drive = path.substr(0, drive_end);
	if (!DoesDriveExist(drive)) {
		spdlog::error("Kernel: unknown drive {}", drive);
		return SCE_KERNEL_ERROR_NODEV;
	}

	auto& file_system = drives[drive];
	auto file = file_system->OpenFile(relative_path, flags);
	if (!file) {
		return SCE_ERROR_ERRNO_ENOENT;
	}

	return AddKernelObject(std::move(file));
}

int Kernel::OpenDirectory(std::string path) {
	int drive_end = path.find('/');
	if (drive_end == -1) drive_end = path.size();
	std::string relative_path = drive_end == path.size() ? "" : path.substr(drive_end + 1);
	std::string drive = path.substr(0, drive_end);
	if (!DoesDriveExist(drive)) {
		spdlog::error("Kernel: unknown drive {}", drive);
		return SCE_KERNEL_ERROR_NODEV;
	}

	auto& file_system = drives[drive];
	auto directory = file_system->OpenDirectory(relative_path);
	if (!directory) {
		return SCE_ERROR_ERRNO_ENOENT;
	}

	return AddKernelObject(std::move(directory));
}

int Kernel::CreateDirectory(std::string path) {
	int drive_end = path.find('/');
	std::string relative_path = path.substr(drive_end + 1);
	std::string drive = path.substr(0, drive_end);
	if (!DoesDriveExist(drive)) {
		spdlog::error("Kernel: unknown drive {}", drive);
		return SCE_KERNEL_ERROR_NODEV;
	}

	auto& file_system = drives[drive];
	file_system->CreateDirectory(relative_path);

	return 0;
}

int Kernel::GetStat(std::string path, SceIoStat* data) {
	int drive_end = path.find('/');
	std::string relative_path = path.substr(drive_end + 1);
	std::string drive = path.substr(0, drive_end);
	if (!DoesDriveExist(drive)) {
		spdlog::error("Kernel: unknown drive {}", drive);
		return SCE_KERNEL_ERROR_NODEV;
	}

	auto& file_system = drives[drive];
	if (!file_system->GetStat(relative_path, data)) {
		return SCE_ERROR_ERRNO_ENOENT;
	}
	return 0;
}

int Kernel::RemoveFile(std::string path) {
	int drive_end = path.find('/');
	std::string relative_path = path.substr(drive_end + 1);
	std::string drive = path.substr(0, drive_end);
	if (!DoesDriveExist(drive)) {
		spdlog::error("Kernel: unknown drive {}", drive);
		return SCE_KERNEL_ERROR_NODEV;
	}

	auto& file_system = drives[drive];
	if (!file_system->RemoveFile(relative_path)) {
		return SCE_ERROR_ERRNO_ENOENT;
	}
	return 0;
}

int Kernel::RemoveDirectory(std::string path) {
	int drive_end = path.find('/');
	std::string relative_path = path.substr(drive_end + 1);
	std::string drive = path.substr(0, drive_end);
	if (!DoesDriveExist(drive)) {
		spdlog::error("Kernel: unknown drive {}", drive);
		return SCE_KERNEL_ERROR_NODEV;
	}

	auto& file_system = drives[drive];
	if (!file_system->RemoveDirectory(relative_path)) {
		return SCE_ERROR_ERRNO_ENOENT;
	}
	return 0;
}

int Kernel::Rename(std::string old_path, std::string new_path) {
	int old_drive_end = old_path.find('/');
	std::string old_relative_path = old_path.substr(old_drive_end + 1);
	std::string old_drive = old_path.substr(0, old_drive_end);
	if (!DoesDriveExist(old_drive)) {
		spdlog::error("Kernel: unknown drive {}", old_drive);
		return SCE_KERNEL_ERROR_NODEV;
	}

	int new_drive_end = new_path.find('/');
	std::string new_relative_path = new_path.substr(new_drive_end + 1);
	std::string new_drive = new_path.substr(0, new_drive_end);
	if (old_drive != new_drive) {
		return SCE_KERNEL_ERROR_XDEV;
	}

	auto& file_system = drives[old_drive];
	if (!file_system->Rename(old_relative_path, new_relative_path)) {
		return SCE_ERROR_ERRNO_ENOENT;
	}
	return 0;
}

void Kernel::WakeUpThread(int thid, WaitReason reason) {
	Thread* thread = GetKernelObject<Thread>(thid);
	if (thread->GetState() != ThreadState::WAIT || thread->GetWaitReason() != reason) { return; }
	thread->SetState(ThreadState::READY);
	thread->SetWaitReason(WaitReason::NONE);

	thread_ready_queue[thread->GetPriority()].push_back(thid);
	if (current_thread == -1) {
		RescheduleNextCycle();
	}
}

void Kernel::WaitCurrentThread(WaitReason reason, bool allow_callbacks) {
	Thread* thread = GetKernelObject<Thread>(current_thread);
	
	auto& queue = thread_ready_queue[thread->GetPriority()];
	queue.erase(std::remove(queue.begin(), queue.end(), current_thread), queue.end());

	thread->SetState(ThreadState::WAIT);
	thread->SetWaitReason(reason);
	thread->SetAllowCallbacks(allow_callbacks);
	RescheduleNextCycle();
}

void Kernel::ExecHLEFunction(int import_index) {
	auto& import_data = hle_imports[import_index];
	auto& func = hle_modules[import_data.module][import_data.nid];

	auto cpu = PSP::GetInstance()->GetCPU();
	func(cpu);
}
