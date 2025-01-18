#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <elfio/elfio.hpp>

#include "psp.hpp"

int main(int argc, char* argv[]) {
    CLI::App app{ "PSX Emulator" };
    argv = app.ensure_utf8(argv);

    std::string elf_path;
    app.add_option("-f,--file", elf_path, "Path to the game ELF")->check(CLI::ExistingFile)->required();
    
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

    CLI11_PARSE(app, argc, argv);

    spdlog::set_level(level);
    
    PSP psp;
    if (!psp.LoadExec(elf_path))
        return 1;
    psp.Run();

    return 0;
}