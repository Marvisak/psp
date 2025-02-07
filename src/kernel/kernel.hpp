#pragma once

#include <type_traits>
#include <fstream>
#include <memory>
#include <queue>
#include <array>
#include <map>

#include "../hle/defs.hpp"
#include "memory.hpp"

#undef CALLBACK

constexpr auto UID_COUNT = 4096;

enum class KernelObjectType {
	INVALID,
	MODULE,
	THREAD,
	CALLBACK,
	MEMORY_BLOCK,
	FILE,
	DIRECTORY
};

class KernelObject {
public:
	virtual ~KernelObject() {}

	virtual std::string GetName() const { return "INVALID"; }
	virtual KernelObjectType GetType() { return KernelObjectType::INVALID; }
	static KernelObjectType GetStaticType() { return KernelObjectType::INVALID; }

	void SetUID(int uid) { this->uid = uid; }
	int GetUID() const { return uid; }
private:
	int uid = -1;
};

class FileSystem;
class Thread;
enum class WaitReason;
class Kernel {
public:
	Kernel();
	~Kernel();

	int AddKernelObject(std::unique_ptr<KernelObject> obj);
	void RemoveKernelObject(int uid) { objects[uid] = nullptr; }
	template <class T>
	requires std::is_base_of_v<KernelObject, T>
	T* GetKernelObject(int uid) {
		if (uid < 0 || uid >= objects.size()) return nullptr;
		auto& object = objects[uid];
		if (!object) return nullptr;
		if (T::GetStaticType() != object->GetType()) return nullptr;
		return reinterpret_cast<T*>(object.get());
	}
	
	int LoadModule(std::string path);
	bool ExecModule(int uid);

	int Reschedule();
	void RescheduleNextCycle() { reschedule_next_cycle = true; }
	bool ShouldReschedule() const { return reschedule_next_cycle; }
	int CreateThread(std::string name, uint32_t entry, int init_priority, uint32_t stack_size, uint32_t attr);
	void DeleteThread(int thid);
	void StartThread(int thid);

	int CreateCallback(std::string name, uint32_t entry, uint32_t common);
	void ExecuteCallback(int cbid);

	void Mount(std::string mount_point, std::shared_ptr<FileSystem> file_system);
	bool DoesDriveExist(std::string mount_point) { return drives.contains(mount_point); }
	int OpenFile(std::string path, int flags);
	int OpenDirectory(std::string path);
	int CreateDirectory(std::string path);
	int GetStat(std::string path, SceIoStat* data);
	int Rename(std::string old_path, std::string new_path);
	int RemoveFile(std::string path);
	int RemoveDirectory(std::string path);

	void WakeUpThread(int thid, WaitReason reason);
	void WaitCurrentThread(WaitReason reason, bool allow_callbacks);

	void WakeUpVBlank();

	void ExecHLEFunction(int import_index);

	int GetExecModule() const { return exec_module; }
	int GetCurrentThread() const { return current_thread; }
	int GetCurrentCallback() const { return current_callback; }
	MemoryAllocator* GetUserMemory() { return user_memory.get(); }
	MemoryAllocator* GetKernelMemory() { return kernel_memory.get(); }
private:
	int exec_module = -1;
	int current_thread = -1;
	int current_callback = -1;
	int next_uid = 1;
	bool reschedule_next_cycle = false;

	std::map<std::string, std::shared_ptr<FileSystem>> drives;
	std::unique_ptr<MemoryAllocator> user_memory;
	std::unique_ptr<MemoryAllocator> kernel_memory;
	std::array<std::unique_ptr<KernelObject>, UID_COUNT> objects{};
	std::array<std::queue<int>, 111> thread_ready_queue{};
};