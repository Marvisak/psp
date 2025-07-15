#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "defs.hpp"

static int sceKernelStdin() {
	return STDIN;
}

static int sceKernelStdout() {
	return STDOUT;
}

static int sceKernelStderr() {
	return STDERR;
}

FuncMap RegisterStdioForUser() {
	FuncMap funcs;
	funcs[0x172D316E] = HLEWrap(sceKernelStdin);
	funcs[0xA6BAB2E9] = HLEWrap(sceKernelStdout);
	funcs[0xF78BA90A] = HLEWrap(sceKernelStderr);
	return funcs;
}