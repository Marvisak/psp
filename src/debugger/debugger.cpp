#include "debugger.hpp"

#include <format>
#include <spdlog/spdlog.h>

#include "../psp.hpp"
#include "../kernel/module.hpp"
#include "../kernel/thread.hpp"

constexpr const char TARGET_DESCRIPTION[] =
#include "target.xml"
;

constexpr const char MEMORY_MAP[] =
#include "memory.xml"
;

Debugger::Debugger(int port) {
	sockpp::initialize();
	listener = sockpp::tcp_acceptor(port);
}

void Debugger::Run() {
	if (!WaitForConnection()) {
		return;
	}

	auto psp = PSP::GetInstance();
	while (true) {
		std::string packet = ReceivePacket();
		if (!packet.empty()) {
			auto response = HandlePacket(packet);
			SendResponse(response);
		}
	}
}

bool Debugger::WaitForConnection() {
	spdlog::info("Debugger: waiting for GDB connection...");
	auto response = listener.accept();
	if (!response) {
		spdlog::error("Debugger: invalid connection");
		return false;
	}
	socket = response.release();
	spdlog::info("Debugger: connection estabilished");

	return true;
}

std::string Debugger::ReceivePacket() {
	std::string packet{};

	char buf;
	auto response = socket.recv(&buf, 1);

	if (response.value() == 0) {
		spdlog::info("Debugger: GDB connection closed");
		WaitForConnection();
		return packet;
	}

	if (buf != '$') {
		return packet;
	}

	while (true) {
		socket.recv(&buf, 1, MSG_WAITALL);
		if (buf != '#') {
			packet += buf;
		} else {
			char checksum_str[3];
			socket.recv(checksum_str, 2, MSG_WAITALL);
			checksum_str[2] = '\0';
			
			uint8_t checksum = strtol(checksum_str, nullptr, 16);
			if (checksum == CalculateChecksum(packet)) {
				socket.send("+");
			} else {
				socket.send("-");
				packet = {};
			}
			break;
		}
	}

	return packet;
}

std::string Debugger::HandlePacket(std::string packet) {
	switch (packet[0]) {
	case '?': return "S00";
	case 'g': return ReadRegisters();
	case 'H': return "OK";
	case 'm': return ReadMemory(packet);
	case 'p': return ReadRegister(packet);
	case 'q': return HandleQueryPacket(packet);
	case 'v': return HandleVPacket(packet);
	case 'z': return RemoveBreakpoint(packet);
	case 'Z': return SetBreakpoint(packet);
	default:
		spdlog::warn("Debugger: unknown GDB packet {}", packet);
		return {};
	}
}

void Debugger::SendResponse(std::string response) {
	int check_sum = CalculateChecksum(response);

	std::string packet = std::format("${}#{:02x}", response, check_sum);

	socket.write(packet.data(), packet.length());
	
	char buffer;
	socket.read(&buffer, 1);
	while (buffer == '-') {
		socket.write(packet.data(), packet.length());
		socket.read(&buffer, 1);
	}
}

uint8_t Debugger::CalculateChecksum(std::string packet) {
	uint32_t sum = 0;
	for (auto c : packet) {
		sum += c;
	}
	return sum % 256;
}

std::string Debugger::ReadRegisters() {
	std::string registers;
	auto cpu = PSP::GetInstance()->GetCPU();
	for (int i = 0; i < 32; i++) {
		registers += ToLittleEndian(cpu->GetRegister(i));
	}

	registers += "xxxxxxxx";
	registers += ToLittleEndian(cpu->GetLO());
	registers += ToLittleEndian(cpu->GetHI());
	registers += "xxxxxxxx";
	registers += "xxxxxxxx";
	registers += ToLittleEndian(cpu->GetPC());

	auto fpu_regs = cpu->GetFPURegs();
	for (auto reg : fpu_regs) {
		auto value = std::bit_cast<uint32_t>(reg);
		registers += ToLittleEndian(value);
	}
	registers += ToLittleEndian(cpu->GetFCR31());
	registers += "xxxxxxxx";

	return registers;
}

std::string Debugger::ReadRegister(std::string packet) {
	int reg = stoi(packet.substr(1), nullptr, 16);

	auto cpu = PSP::GetInstance()->GetCPU();
	if (reg < 32) {
		return ToLittleEndian(cpu->GetRegister(reg));
	} else if (reg > 37 && reg < 70) {
		auto value = std::bit_cast<uint32_t>(cpu->GetFPURegs()[reg - 38]);
		return ToLittleEndian(value);
	} else if (reg == 33) {
		return ToLittleEndian(cpu->GetLO());
	} else if (reg == 34) {
		return ToLittleEndian(cpu->GetHI());
	} else if (reg == 37) {
		return ToLittleEndian(cpu->GetPC());
	} else {
		return "xxxxxxxx";
	}
}

std::string Debugger::ReadMemory(std::string packet) {
	std::stringstream ss(packet.substr(1));

	char ch;
	uint32_t addr, len;
	ss >> std::hex >> addr >> ch >> len;

	auto real_addr = reinterpret_cast<uint8_t*>(PSP::GetInstance()->VirtualToPhysical(addr));
	if (!real_addr)
		return "E00";

	std::string data{};
	for (int i = 0; i < len; i++) {
		data += std::format("{:02x}", real_addr[i]);
	}

	return data;
}

std::string Debugger::SetBreakpoint(std::string packet) {
	std::stringstream ss(packet.substr(3));
	uint32_t addr;
	ss >> std::hex >> addr;

	if (PSP::GetInstance()->VirtualToPhysical(addr) == 0) {
		return "E00";
	}

	if (packet.starts_with("Z0")) {
		breakpoints.push_back(addr);
	}
	else {
		spdlog::warn("Debugger: unknown breakpoint packet {}", packet);
		return {};
	}

	return "OK";
}

std::string Debugger::RemoveBreakpoint(std::string packet) {
	std::stringstream ss(packet.substr(3));
	uint32_t addr;
	ss >> std::hex >> addr;

	if (PSP::GetInstance()->VirtualToPhysical(addr) == 0) {
		return "E00";
	}

	if (packet.starts_with("z0")) {
		breakpoints.erase(std::remove(breakpoints.begin(), breakpoints.end(), addr), breakpoints.end());
	}
	else {
		spdlog::warn("Debugger: unknown breakpoint packet {}", packet);
		return {};
	}

	return "OK";
}

std::string Debugger::HandleQueryPacket(std::string packet) {
	if (packet.starts_with("qSupported")) {
		return "PacketSize=1024;qXfer:features:read+;qXfer:memory-map:read+";
	} else if (packet.starts_with("qAttached")) {
		return "1";
	} else if (packet.starts_with("qSymbol")) {
		return "OK";
	} else if (packet.starts_with("qOffsets")) {
		auto kernel = PSP::GetInstance()->GetKernel();
		int mid = kernel->GetExecModule();
		auto module = kernel->GetKernelObject<Module>(mid);
		if (!module) {
			return {};
		}
		uint32_t offset = module->GetOffset();

		return std::format("Text={0:02x};Data={0:02x};Bss={0:02x}", offset);
	} else if (packet.starts_with("qXfer")) {
		if (packet.starts_with("qXfer:features:read:target.xml")) {
			return TARGET_DESCRIPTION;
		} else if (packet.starts_with("qXfer:memory-map:read:")) {
			return std::format(MEMORY_MAP, VRAM_START, VRAM_END - VRAM_START, KERNEL_MEMORY_START, RAM_SIZE);
		} else {
			spdlog::warn("Debugger: unsupported qXfer {}", packet);
			return {};
		}
	} else {
		spdlog::warn("Debugger: unknown query {}", packet);
		return {};
	}
}

std::string Debugger::HandleVPacket(std::string packet) {
	std::string command = packet.substr(0, packet.find(";"));
	if (command == "vMustReplyEmpty") {
		return {};
	} else if (command == "vCont?") {
		return "vCont;s;S;c;C;";
	} else if (command == "vCont") {
		auto psp = PSP::GetInstance();
		switch (packet[6]) {
		case 's':
			psp->Step();
			psp->GetRenderer()->Frame();
			return "S05";
		case 'c': {
			while (true) {
				uint32_t pc = psp->GetCPU()->GetPC();
				for (auto breakpoint : breakpoints) {
					if (breakpoint == pc) {
						psp->GetRenderer()->Frame();
						return "S05";
					}
				}

				psp->Step();

				if (psp->IsClosed()) {
					return "S03";
				}
			}
			break;
		}

		default:
			spdlog::warn("Debugger: unknown vCont command {}", packet);
			return {};
		}
	}
	spdlog::warn("Debugger: unknown v command {}", command);
	return {};
}

std::string Debugger::ToLittleEndian(uint32_t value) {
	return std::format("{:02x}{:02x}{:02x}{:02x}",
		value & 0xFF,
		(value >> 8) & 0xFF,
		(value >> 16) & 0xFF,
		(value >> 24) & 0xFF);
}