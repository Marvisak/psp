#pragma once

#include <vector>
#include <queue>
#include <sockpp/tcp_acceptor.h>

class Debugger {
public:
	Debugger(int port);

	void Run();
	bool WaitForConnection();
	std::string ReceivePacket();
	std::string HandlePacket(std::string packet);
	void SendResponse(std::string response);
	uint8_t CalculateChecksum(std::string packet);

	std::string ReadRegisters();
	std::string ReadRegister(std::string packet);
	std::string ReadMemory(std::string packet);
	std::string SetBreakpoint(std::string packet);
	std::string RemoveBreakpoint(std::string packet);
	std::string HandleQueryPacket(std::string packet);
	std::string HandleVPacket(std::string packet);

	std::string ToLittleEndian(uint32_t value);
private:
	std::vector<uint32_t> breakpoints;

	sockpp::tcp_acceptor listener;
	sockpp::tcp_socket socket;
};