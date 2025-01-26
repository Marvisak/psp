#pragma once

#include <type_traits>
#include <fstream>
#include <queue>
#include <array>

#include "memory.hpp"

enum class KernelObjectType {
	INVALID,
	MODULE,
	THREAD
};

class KernelObject {
public:
	virtual std::string GetName() const { return "INVALID"; }
	virtual KernelObjectType GetType() { return KernelObjectType::INVALID; }
	static KernelObjectType GetStaticType() { return KernelObjectType::INVALID; }

	void SetUID(int uid) { this->uid = uid; }
	int GetUID() { return uid; }
protected:
	int uid;
};

class Thread;
enum class WaitReason;
class Kernel {
public:
	Kernel();
	void AllocateFakeSyscalls();

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
	int StartThread(int thid);

	void WakeUpThread(int thid, WaitReason reason);
	void WaitCurrentThread(WaitReason reason);

	void WakeUpVBlank();

	void ExecHLEFunction(int import_index);

	int GetCurrentThread() const { return current_thread; }
	MemoryAllocator& GetUserMemory() { return user_memory; }
	MemoryAllocator& GetKernelMemory() { return kernel_memory; }
private:
	int current_thread = -1;
	int next_uid = 0;

	std::array<std::unique_ptr<KernelObject>, 4096> objects{};
	std::array<std::queue<int>, 111> thread_ready_queue{};
	MemoryAllocator user_memory;
	MemoryAllocator kernel_memory;
};