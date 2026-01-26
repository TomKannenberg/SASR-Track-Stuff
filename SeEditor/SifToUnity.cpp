#include "SeEditor/UnityExport.hpp"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cout << "sif_to_unity <input.sif> [<unity_project_root>]\n";
        std::cout << "Writes: <unity_project_root>/Assets/<SIFNAME>.SIF/export.json and mesh/collision .obj files.\n";
        return 1;
    }

    std::filesystem::path inputPath = argv[1];
    std::filesystem::path unityRoot;
    if (argc >= 3)
        unityRoot = std::filesystem::path(argv[2]);
    else
        unityRoot = std::filesystem::current_path() / ".." / ".." / "Unity";

    auto res = SeEditor::UnityExport::ExportSifToUnity(inputPath, unityRoot);
    if (!res.Success)
    {
        std::cerr << "[sif_to_unity] " << res.Error << "\n";
        return 2;
    }

    std::cout << "[sif_to_unity] Wrote " << res.ExportJsonPath.string() << "\n";
    return 0;
}

