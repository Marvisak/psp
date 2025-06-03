#include <filesystem>
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <elfio/elfio.hpp>

#include "debugger/debugger.hpp"
#include "psp.hpp"

int main(int argc, char* argv[]) {
    CLI::App app{ "PSX Emulator" };
    argv = app.ensure_utf8(argv);

    std::string elf_path;
    app.add_option("-f,--file", elf_path, "Path to the game ELF")->check(CLI::ExistingPath)->required();

    std::string memstick_path = (std::filesystem::current_path().parent_path() / "memstick").string();
    app.add_option("-m,--memstick", elf_path, "Path to the memory stick folder");
    
    spdlog::level::level_enum level = spdlog::level::info;
    std::map<std::string, spdlog::level::level_enum> levels{
        {"trace", spdlog::level::trace},
        {"debug", spdlog::level::debug},
        {"info", spdlog::level::info},
        {"warning", spdlog::level::warn},
        {"error", spdlog::level::err},
        {"critical", spdlog::level::critical},
        {"off", spdlog::level::off} };
    app.add_option("-l,--loglevel", level, "Log level")->transform(CLI::CheckedTransformer(levels, CLI::ignore_case));

    RendererType renderer_type = RendererType::COMPUTE;
    std::map<std::string, RendererType> renderer_types{
        {"software", RendererType::SOFTWARE},
        {"compute", RendererType::COMPUTE}
    };
    app.add_option("-r,--renderer", renderer_type, "Renderer type")->transform(CLI::CheckedTransformer(renderer_types, CLI::ignore_case));

    bool enable_debugger = false;
    app.add_flag("-g,--gdb", enable_debugger, "Enable GDB Stub");

    int gdb_port = 5678;
    app.add_option("-p,--port", gdb_port, "Port used by GDB Stub");

    bool nearest_filtering = false;
    app.add_flag("-n,--nearest", nearest_filtering, "Enables nearest screen filtering instead of linear");

    CLI11_PARSE(app, argc, argv);

    spdlog::set_level(level);
    
    PSP psp(renderer_type, nearest_filtering);
    if (!psp.LoadExec(elf_path)) {
        return 1;
    }

    if (!psp.LoadMemStick(memstick_path)) {
        return 1;
    }

    if (enable_debugger) {
       Debugger debugger(gdb_port);

       debugger.Run();
    }
    else {
        psp.Run();
    }

    return 0;
}