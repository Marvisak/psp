#pragma once

#include <cstdint>
#include <array>

#include "kernel.hpp"
#include "module.hpp"

enum class ThreadState {
	RUN = 1,
	READY = 2,
	WAIT = 4,
	SUSPEND = 8,
	DORMANT = 16,
	WAIT_SUSPEND = WAIT | SUSPEND
};

enum class WaitReason {
	NONE = 0,
	SLEEP = 1,
	DELAY = 2,
	SEMAPHORE = 3,
	VBLANK = 6,
	THREAD_END = 9,
	UMD = 11,
	MUTEX = 13,
	LWMUTEX = 14,
	CTRL = 15,
	IO = 16,
	DRAW_SYNC = 17,
	LIST_SYNC = 18,
	HLE_DELAY = 20
};

struct WaitObject {
	bool ended;
	WaitReason reason;
};

class Thread : public KernelObject {
public:
	Thread(Module* module, std::string name, uint32_t entry_addr, int priority, uint32_t stack_size, uint32_t attr, uint32_t return_addr);
	~Thread();

	void Start(int arg_size, uint32_t arg_block_addr);
	
	void CreateStack();
	void FillStack();

	void SwitchState();
	void SaveState();
	
	bool IsCallbackPending(int cbid) const { return std::find(pending_callbacks.begin(), pending_callbacks.end(), cbid) != pending_callbacks.end(); }
	bool IsCallbackRunning(int cbid) const { return std::find(current_callbacks.begin(), current_callbacks.end(), cbid) != current_callbacks.end(); }
	bool HasPendingCallback() const { return !pending_callbacks.empty(); }
	void AddPendingCallback(int cbid) { pending_callbacks.push_back(cbid); }
	int GetCurrentCallback() const {
		if (current_callbacks.empty()) {
			return -1;
		}
		return current_callbacks.front();
	}

	void WakeUpForCallback();
	void RestoreFromCallback(std::shared_ptr<WaitObject> wait);

	int GetWakeupCount() const { return wakeup_count; }
	void DecWakeupCount() { wakeup_count--; }
	void IncWakeupCount() { wakeup_count++; }

	int GetModule() const { return modid; }
	int GetInitPriority() const { return init_priority; }
	uint32_t GetEntry() const { return entry; }
	uint32_t GetStack() const { return initial_stack; }
	uint32_t GetStackSize() const { return stack_size; }

	int GetPriority() const { return priority; }
	void SetPriority(int priority) { this->priority = priority; }

	uint32_t GetAttr() const { return attr; }
	void SetAttr(uint32_t attr) { this->attr = attr; }

	void SetReturnValue(uint32_t value) {
		auto psp = PSP::GetInstance();
		auto current_thread = psp->GetKernel()->GetCurrentThread();
		if (current_thread == GetUID()) {
			psp->GetCPU()->SetRegister(MIPS_REG_V0, value);
		}
		cpu_state.regs[MIPS_REG_V0] = value;
	}

	uint32_t GetGP() const { return gp; }
	void SetGP(uint32_t gp) { this->gp = gp; }

	uint32_t GetExitStatus() const { return exit_status; }
	void SetExitStatus(int exit_status) { this->exit_status = exit_status; }

	void ClearWait() { wait = nullptr; }
	WaitReason GetWaitReason() {
		if (!wait || wait->ended) {
			wait = nullptr;
			return WaitReason::NONE;
		}
		return wait->reason;
	}

	std::shared_ptr<WaitObject> Wait(WaitReason reason) {
		wait = std::make_shared<WaitObject>();
		wait->reason = reason;
		wait->ended = false;
		return wait;
	}

	ThreadState GetState() const { return state; }
	void SetState(ThreadState state) { this->state = state; }

	bool GetAllowCallbacks() const { return allow_callbacks; }
	void SetAllowCallbacks(bool allow_callbacks) { this->allow_callbacks = allow_callbacks; }

	std::string GetName() const { return name; }
	KernelObjectType GetType() override { return KernelObjectType::THREAD; }
	static KernelObjectType GetStaticType() { return KernelObjectType::THREAD; }
private:
	std::string name = "root";

	ThreadState state = ThreadState::DORMANT;
	std::shared_ptr<WaitObject> wait{};
	int wakeup_count = 0;

	bool allow_callbacks = false;
	std::shared_ptr<WaitObject> pending_wait{};
	std::deque<int> pending_callbacks{};
	std::deque<int> current_callbacks{};

	int modid = 0;
	uint32_t gp = 0;
	uint32_t attr;
	uint32_t entry;
	uint32_t return_addr;
	int init_priority;
	int priority;
	uint32_t initial_stack;
	uint32_t stack_size;
	uint32_t exit_status = SCE_KERNEL_ERROR_DORMANT;

	CPUState cpu_state{};
};