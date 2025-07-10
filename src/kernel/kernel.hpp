#pragma once

#include <unordered_map>
#include <type_traits>
#include <fstream>
#include <memory>
#include <vector>
#include <array>

#include "../hle/defs.hpp"
#include "memory.hpp"

#undef CALLBACK

constexpr auto UID_COUNT = 4096;

enum class KernelObjectType {
	INVALID = 0,
	THREAD = 1,
	SEMAPHORE = 2,
	CALLBACK = 8,
	MUTEX = 12,
	MODULE = 0x1000,
	MEMORY_BLOCK = 0x1001,
	FILE = 0x1002,
	DIRECTORY = 0x1003,
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
struct WaitObject;
class Kernel {
public:
	Kernel();
	~Kernel();

	int AddKernelObject(std::unique_ptr<KernelObject> obj);
	void RemoveKernelObject(int uid);
	void ClearSortedObjects() { sorted_objects.clear(); }
	std::vector<int> GetKernelObjects(KernelObjectType type) { return sorted_objects[type]; }

	template <class T>
	requires std::is_base_of_v<KernelObject, T>
	T* GetKernelObject(int uid) {
		if (uid < 0 || uid >= objects.size()) return nullptr;
		auto& object = objects[uid];
		if (!object) return nullptr;
		if (T::GetStaticType() != object->GetType()) return nullptr;
		return reinterpret_cast<T*>(object.get());
	}

	KernelObject* GetKernelObject(int uid) {
		if (uid < 0 || uid >= objects.size()) return nullptr;
		auto& object = objects[uid];
		if (!object) return nullptr;
		return object.get();
	}
	
	int LoadModule(std::string path);
	bool ExecModule(int uid);

	int GetSDKVersion() const { return sdk_version; }
	void SetSDKVersion(int version) { sdk_version = version; }

	int Reschedule();
	void ForceReschedule() { force_reschedule = true; HLEReschedule(); }
	void HLEReschedule() { reschedule = true; }
	void AddThreadToQueue(int thid);
	void RemoveThreadFromQueue(int thid);
	void RotateThreadReadyQueue(int priority);

	int CreateThread(std::string name, uint32_t entry, int init_priority, uint32_t stack_size, uint32_t attr);
	void StartThread(int thid, int arg_size, uint32_t arg_block_addr);

	int CreateCallback(std::string name, uint32_t entry, uint32_t common);
	void CheckCallbacks();
	bool CheckCallbacksOnThread(int thid);

	int CreateSemaphore(std::string name, uint32_t attr, int init_count, int max_count);
	int CreateMutex(std::string name, uint32_t attr, int init_count);

	void Mount(std::string mount_point, std::shared_ptr<FileSystem> file_system);
	bool DoesDriveExist(std::string mount_point) { return drives.contains(mount_point); }
	int OpenFile(std::string path, int flags);
	int OpenDirectory(std::string path);
	int CreateDirectory(std::string path);
	int GetStat(std::string path, SceIoStat* data);
	int Rename(std::string old_path, std::string new_path);
	int RemoveFile(std::string path);
	int RemoveDirectory(std::string path);
	int FixPath(std::string path, std::string& out);

	bool WakeUpThread(int thid);
	std::shared_ptr<WaitObject> WaitCurrentThread(WaitReason reason, bool allow_callbacks);

	void ExecHLEFunction(int import_index);
	void SkipDeadbeef() { skip_deadbeef = true; }

	void SetInterruptsEnabled(bool interrupts_enabled) { this->interrupts_enabled = interrupts_enabled; }
	bool InterruptsEnabled() const { return interrupts_enabled; }

	int GetExecModule() const { return exec_module; }
	int GetCurrentThread() const { return current_thread; }
	MemoryAllocator* GetUserMemory() { return user_memory.get(); }
	MemoryAllocator* GetKernelMemory() { return kernel_memory.get(); }
private:
	int exec_module = -1;
	int current_thread = -1;
	int next_uid = 1;
	bool reschedule = false;
	bool force_reschedule = false;
	bool skip_deadbeef = false;
	int sdk_version = 0;
	bool interrupts_enabled = true;

	std::unordered_map<std::string, std::shared_ptr<FileSystem>> drives;
	std::unique_ptr<MemoryAllocator> user_memory;
	std::unique_ptr<MemoryAllocator> kernel_memory;
	std::unordered_map<KernelObjectType, std::vector<int>> sorted_objects{};
	std::array<std::unique_ptr<KernelObject>, UID_COUNT> objects{};
	std::array<std::vector<int>, 128> thread_ready_queue{};
};