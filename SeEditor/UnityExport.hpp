#pragma once

#include <filesystem>
#include <string>

namespace SeEditor::UnityExport {

struct ExportResult
{
    bool Success = false;
    std::filesystem::path ExportJsonPath{};
    std::filesystem::path ScenePath{};
    std::string Error;
};

// Exports SIF forests (per-branch meshes), collision and logic manifest.
// `unityProjectRoot` may be either:
// - the actual Unity project folder (must contain Assets/ + ProjectSettings/), OR
// - a user-selected "root folder", in which case the exporter will create/use `<root>/Unity` as the Unity project.
//
// Writes to: <UnityProject>/Assets/<SIFNAME>_SIF/* (Meshes/, Textures/, Prefabs/, collision.obj, <SIFNAME>.unity, export.json).
ExportResult ExportSifToUnity(std::filesystem::path const& sifPath,
                             std::filesystem::path const& unityProjectRoot);

// Finds the repo's Unity project root by walking upwards and checking for a Unity/ folder.
// Returns empty path if not found.
std::filesystem::path FindUnityProjectRoot(std::filesystem::path const& startDir);

} // namespace SeEditor::UnityExport
