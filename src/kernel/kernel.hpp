#pragma once

#include <type_traits>
#include <fstream>
#include <queue>
#include <array>

#include "memory.hpp"

#undef CALLBACK

constexpr auto UID_COUNT = 4096;

enum class KernelObjectType {
	INVALID,
	MODULE,
	THREAD,
	CALLBACK,
	MEMORY_BLOCK
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

class Thread;
enum class WaitReason;
class Kernel {
public:
	Kernel();
	int AddKernelObject(std::unique_ptr<KernelObject> obj);

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
	int CreateThread(std::string name, uint32_t entry, int init_priority, uint32_t stack_size, uint32_t attr);
	void DeleteThread(int thid);
	void StartThread(int thid);

	int CreateCallback(std::string name, uint32_t entry, uint32_t common);
	void ExecuteCallback(int cbid);

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
	int next_uid = 0;

	std::unique_ptr<MemoryAllocator> user_memory;
	std::unique_ptr<MemoryAllocator> kernel_memory;
	std::array<std::unique_ptr<KernelObject>, UID_COUNT> objects{};
	std::array<std::queue<int>, 111> thread_ready_queue{};
};