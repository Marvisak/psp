#include "kernel.hpp"

#include <spdlog/spdlog.h>

#include "../hle/defs.hpp"
#include "../psp.hpp"

#include "mutex.hpp"
#include "module.hpp"
#include "thread.hpp"
#include "callback.hpp"
#include "semaphore.hpp"
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
			sorted_objects[obj->GetType()].push_back(i);
			objects[i] = std::move(obj);
			++next_uid %= objects.size();
			if (next_uid == 0) {
				next_uid++;
			}
			return i;
		}
	}
	spdlog::error("Kernel: unable to assing uid to object");
	return 0;
}

void Kernel::RemoveKernelObject(int uid) {
	auto& object_map = sorted_objects[objects[uid]->GetType()];
	object_map.erase(std::remove(object_map.begin(), object_map.end(), uid), object_map.end());
	objects[uid] = nullptr;
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
	auto psp = PSP::GetInstance();

	auto module = GetKernelObject<Module>(uid);
	if (!module) {
		return false;
	}
	
	std::string file_path = module->GetFilePath();
	auto thread = std::make_unique<Thread>(module, module->GetName(), module->GetEntrypoint(), 0x20, 0x40000, 0, KERNEL_MEMORY_START);

	int thid = AddKernelObject(std::move(thread));
	// The second address is garbage, just so that something is copied there
	StartThread(thid, file_path.size() + 1, module->GetEntrypoint());
	Reschedule();

	auto stack_addr = psp->GetCPU()->GetRegister(MIPS_REG_A1);
	auto stack = psp->VirtualToPhysical(stack_addr);
	memcpy(stack, file_path.data(), file_path.size() + 1);

	exec_module = uid;

	return true;
}

// TODO: Maybe add some cycle counting to this?
int Kernel::Reschedule() {
	CheckCallbacks();

	auto current_thread_obj = GetKernelObject<Thread>(current_thread);
	bool current_thread_valid = !force_reschedule && current_thread_obj && current_thread_obj->GetState() == ThreadState::RUN;
	reschedule = false;
	force_reschedule = false;
	for (int priority = 0; priority < thread_ready_queue.size(); priority++) {
		auto& ready_threads = thread_ready_queue[priority];
		if (ready_threads.empty()) {
			continue;
		}

		if (current_thread_valid && current_thread_obj->GetPriority() <= priority) {
			break;
		}

		Thread* ready_thread = nullptr;
		for (auto thid : ready_threads) {
			auto thread = GetKernelObject<Thread>(thid);
			if (!thread) {
				spdlog::error("Kernel: thread in ready queue doesn't exist");
				continue;
			}
			if (thread->GetState() == ThreadState::READY) {
				ready_thread = thread;
				break;
			}
		}

		if (!ready_thread) {
			continue;
		}

		if (current_thread_obj) {
			if (current_thread_valid) {
				current_thread_obj->SetState(ThreadState::READY);
			}
			current_thread_obj->SaveState();
		}

		ready_thread->SwitchState();
		current_thread = ready_thread->GetUID();
		return current_thread;
	}

	if (!current_thread_valid) {
		if (current_thread_obj) {
			current_thread_obj->SaveState();
		}

		current_thread = -1;

		auto psp = PSP::GetInstance();

		// At this point there is no CPU activity happening
		// We could either wait for something to happen, or just jump through scheduled events until the thread is changed
		while (current_thread == -1 && !psp->IsClosed()) {
			psp->JumpToNextEvent();
		}
	} else if (current_thread_obj->HasPendingCallback()) {
		current_thread_obj->SaveState();
		current_thread_obj->SwitchState();
	}

	return current_thread;
}

void Kernel::AddThreadToQueue(int thid) {
	auto thread = GetKernelObject<Thread>(thid);

	thread_ready_queue[thread->GetPriority()].push_back(thid);
}

void Kernel::RemoveThreadFromQueue(int thid) {
	auto thread = GetKernelObject<Thread>(thid);

	auto& queue = thread_ready_queue[thread->GetPriority()];
	queue.erase(std::remove(queue.begin(), queue.end(), thid), queue.end());
}

void Kernel::RotateThreadReadyQueue(int priority) {
	auto& queue = thread_ready_queue[priority];
	if (queue.empty()) {
		return;
	}

	auto current_thread_obj = GetKernelObject<Thread>(GetCurrentThread());
	if (current_thread_obj->GetPriority() == priority) {
		RemoveThreadFromQueue(GetCurrentThread());
		AddThreadToQueue(GetCurrentThread());
		current_thread_obj->SetState(ThreadState::READY);
	} else {
		std::rotate(queue.begin(), queue.end() - 1, queue.end());
	}
}

int Kernel::CreateThread(std::string name, uint32_t entry, int init_priority, uint32_t stack_size, uint32_t attr) {
	auto current_thread_obj = GetKernelObject<Thread>(GetCurrentThread());
	auto module = GetKernelObject<Module>(current_thread_obj->GetModule());
	auto thread = std::make_unique<Thread>(module, name, entry, init_priority, stack_size, attr, KERNEL_MEMORY_START + 8);
	return AddKernelObject(std::move(thread));
}

void Kernel::StartThread(int thid, int arg_size, uint32_t arg_block_addr) {
	auto thread = GetKernelObject<Thread>(thid);

	thread->Start(arg_size, arg_block_addr);
	thread->SetState(ThreadState::READY);
	AddThreadToQueue(thid);
}

int Kernel::CreateCallback(std::string name, uint32_t entry, uint32_t common) {
	auto callback = std::make_unique<Callback>(GetCurrentThread(), name, entry, common);
	return AddKernelObject(std::move(callback));
}

void Kernel::CheckCallbacks() {
	auto callbacks = GetKernelObjects(KernelObjectType::CALLBACK);
	for (auto cbid : callbacks) {
		auto callback = GetKernelObject<Callback>(cbid);
		if (callback->GetNotifyCount() == 0) {
			continue;
		}

		auto thread = GetKernelObject<Thread>(callback->GetThread());
		if (!thread->GetAllowCallbacks()) {
			continue;
		}

		if (thread->IsCallbackPending(cbid) || thread->IsCallbackRunning(cbid)) {
			continue;
		}

		thread->AddPendingCallback(cbid);
		if (thread->GetState() != ThreadState::READY) {
			AddThreadToQueue(callback->GetThread());
		}
		thread->WakeUpForCallback();
	}
}

bool Kernel::CheckCallbacksOnThread(int thid) {
	bool executed = false;
	auto callbacks = GetKernelObjects(KernelObjectType::CALLBACK);
	for (auto cbid : callbacks) {
		auto callback = GetKernelObject<Callback>(cbid);
		if (callback->GetNotifyCount() == 0 || callback->GetThread() != thid) {
			continue;
		}

		auto thread = GetKernelObject<Thread>(thid);
		if (thread->IsCallbackPending(cbid) || thread->IsCallbackRunning(cbid)) {
			continue;
		}

		executed = true;
		thread->AddPendingCallback(cbid);
		thread->WakeUpForCallback();
	}
	return executed;
}

int Kernel::CreateSemaphore(std::string name, uint32_t attr, int init_count, int max_count) {
	auto semaphore = std::make_unique<Semaphore>(name, attr, init_count, max_count);
	return AddKernelObject(std::move(semaphore));
}

int Kernel::CreateMutex(std::string name, uint32_t attr, int init_count) {
	auto mutex = std::make_unique<Mutex>(name, attr, init_count);
	return AddKernelObject(std::move(mutex));
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
	std::string relative_path = path.substr(drive_end + 1);
	std::string drive = path.substr(0, drive_end);
	if (drive_end == -1) {
		relative_path = "";
		drive = path;
	}

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
	if (drive_end == -1) {
		relative_path = "";
		drive = path;
	}

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

int Kernel::FixPath(std::string path, std::string& out) {
	int drive_end = path.find('/');
	std::string relative_path = path.substr(drive_end + 1);
	std::string drive = path.substr(0, drive_end);
	if (drive_end == -1) {
		relative_path = "";
		drive = path;
	}

	if (!DoesDriveExist(drive)) {
		spdlog::error("Kernel: unknown drive {}", drive);
		return SCE_KERNEL_ERROR_NODEV;
	}

	auto& file_system = drives[drive];
	out = drive + "/" + file_system->FixPath(relative_path);
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
	return file_system->Rename(old_relative_path, new_relative_path);
}

bool Kernel::WakeUpThread(int thid) {
	Thread* thread = GetKernelObject<Thread>(thid);

	if (thread->GetWaitReason() != WaitReason::NONE) {
		return false;
	}

	if (thread->GetState() == ThreadState::WAIT_SUSPEND) {
		thread->SetState(ThreadState::SUSPEND);
	} else if (thread->GetState() == ThreadState::WAIT) {
		thread->SetState(ThreadState::READY);
		AddThreadToQueue(thid);
	}

	thread->SetAllowCallbacks(false);
	if (GetCurrentThread() == -1) {
		Reschedule();
	}

	return true;
}

std::shared_ptr<WaitObject> Kernel::WaitCurrentThread(WaitReason reason, bool allow_callbacks) {
	auto thread = GetKernelObject<Thread>(GetCurrentThread());

	thread->SetState(ThreadState::WAIT);
	RemoveThreadFromQueue(GetCurrentThread());
	thread->SetAllowCallbacks(allow_callbacks);
	HLEReschedule();

	return thread->Wait(reason);
}

void Kernel::ExecHLEFunction(int import_index) {
	auto& import_data = hle_imports[import_index];
	auto& module = hle_modules[import_data.module];
	if (!module.contains(import_data.nid)) {
		spdlog::error("Kernel: calling unimplemented {} {:x}", import_data.module, import_data.nid);
		return;
	}
	auto& func = hle_modules[import_data.module][import_data.nid];

	auto psp = PSP::GetInstance();
	auto cpu = psp->GetCPU();
	func(cpu);

	if (!skip_deadbeef) {
		auto state = cpu->GetState();
		for (int i = MIPS_REG_A0; i <= MIPS_REG_T7; i++) {
			state.regs[i] = 0xDEADBEEF;
		}
		state.regs[MIPS_REG_AT] = 0xDEADBEEF;
		state.regs[MIPS_REG_T8] = 0xDEADBEEF;
		state.regs[MIPS_REG_T9] = 0xDEADBEEF;
		state.hi = 0xDEADBEEF;
		state.lo = 0xDEADBEEF;
		cpu->SetState(state);
	}
	skip_deadbeef = false;

	if (reschedule) {
		psp->ExecuteEvents();
		Reschedule();
	}
}
