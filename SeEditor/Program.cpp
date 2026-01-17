#include "Program.hpp"

#include "CharmyBee.hpp"
#include "SifParser.hpp"
#include "Frontend.hpp"
#include "Editor/Scene.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <optional>
#include <span>
#include <string>
#include <iomanip>

namespace SeEditor {

int Program::Run(int argc, char** argv)
{
    bool debugKeyInput = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--debug-keyinput")
            debugKeyInput = true;
    }

    if (argc >= 3 && std::string(argv[1]) == "--parse-sif")
    {
        std::filesystem::path path = argv[2];
        std::cout << "[SIFDump] Loading " << path.string() << std::endl;

        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            std::cerr << "[SIFDump] Failed to open file." << std::endl;
            return 1;
        }

        std::vector<char> buffer((std::istreambuf_iterator<char>(file)), {});
        std::vector<std::uint8_t> data(buffer.begin(), buffer.end());

        std::string error;
        auto parsed = ParseSifFile(std::span<const std::uint8_t>(data.data(), data.size()), error);
        if (!parsed)
        {
            std::cerr << "[SIFDump] Parse error: " << error << std::endl;
            return 2;
        }

        std::cout << "[SIFDump] Original size: " << data.size() << " bytes\n";
        if (parsed->WasCompressed)
            std::cout << "[SIFDump] Decompressed payload: " << parsed->DecompressedSize << " bytes\n";
        std::cout << "[SIFDump] Chunks: " << parsed->Chunks.size() << "\n";
        for (std::size_t i = 0; i < parsed->Chunks.size(); ++i)
        {
            auto const& c = parsed->Chunks[i];
            std::cout << "  [" << std::setw(2) << i << "] "
                      << c.Name << " (0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
                      << c.TypeValue << std::dec << std::nouppercase << "): data=" << c.DataSize
                      << " chunk=" << c.ChunkSize << " reloc=" << c.Relocations.size() << "\n";

            if (c.TypeValue == 0x45524F46) // Forest
            {
                std::cout << "     Forest data bytes: ";
                for (std::size_t j = 0; j < std::min<std::size_t>(16, c.Data.size()); ++j)
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<int>(c.Data[j]) << ' ';
                std::cout << std::dec << std::setfill(' ');
                std::cout << "\n";
                std::cout << "     Relocation offsets: ";
                for (std::size_t j = 0; j < std::min<std::size_t>(16, c.Relocations.size()); ++j)
                    std::cout << c.Relocations[j] << ' ';
                std::cout << "\n";
            }
        }
        return 0;
    }

    std::cout << "Starting SeEditor program..." << std::endl;

    Frontend frontend(1600, 900);
    frontend.SetActiveScene(Editor::Scene());
    frontend.LoadFrontend("data/frontend/ui.pkg");

    CharmyBee editor("SeEditor", 1600, 900, debugKeyInput);
    editor.Run();
    return 0;
}

} // namespace SeEditor
