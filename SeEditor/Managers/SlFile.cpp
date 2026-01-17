#include "SlFile.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <span>
#include <SlLib/Excel/ExcelData.hpp>
#include <SlLib/Resources/Database/SlResourceDatabase.hpp>
#include <SlLib/Utilities/CryptUtil.hpp>

namespace {

std::optional<std::filesystem::path> normalizePath(std::string const& path)
{
    if (std::filesystem::exists(path))
        return std::filesystem::absolute(path);
    return std::nullopt;
}

} // namespace

std::optional<SlLib::Excel::ExcelData> SlFile::GetExcelData(std::string const& path)
{
    static std::array<std::string, 2> suffixes{".zat", ".dat"};
    for (auto const& suffix : suffixes)
    {
        std::string candidate = path + suffix;
        if (auto data = GetFile(candidate))
        {
            if (suffix == ".zat")
                SlLib::Utilities::CryptUtil::DecodeBuffer(std::span<std::uint8_t>(data->data(), data->size()));

            return SlLib::Excel::ExcelData::Load(std::span<const std::uint8_t>(data->data(), data->size()));
        }
    }
    return std::nullopt;
}

std::vector<std::filesystem::path> SlFile::s_gameDataFolders = {};

void SlFile::AddGameDataFolder(std::string const& root)
{
    std::filesystem::path candidate = root;
    if (!std::filesystem::exists(candidate))
    {
        std::cerr << "Game data folder not found: " << root << "\n";
        return;
    }

    s_gameDataFolders.push_back(std::move(candidate));
}

bool SlFile::DoesFileExist(std::string const& path)
{
    return findFile(path).has_value();
}

std::optional<std::vector<std::uint8_t>> SlFile::GetFile(std::string const& path)
{
    std::optional<std::filesystem::path> resolved = findFile(path);
    if (!resolved)
        return std::nullopt;

    std::ifstream input(resolved->string(), std::ios::binary);
    if (!input)
        return std::nullopt;

    input.seekg(0, std::ios::end);
    std::streamsize size = input.tellg();
    input.seekg(0, std::ios::beg);

    if (size <= 0)
        return std::vector<std::uint8_t>{};

    std::vector<std::uint8_t> data(static_cast<size_t>(size));
    input.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

std::optional<SlLib::Resources::Database::SlResourceDatabase> SlFile::GetSceneDatabase(std::string const& path,
                                                                                       std::string const& extension)
{
    if (extension.empty())
        return std::nullopt;

    std::string scenePath = path;
    if (!extension.empty() && !scenePath.empty() && scenePath.back() == '\\')
        scenePath.pop_back();

    std::optional<std::filesystem::path> resolved = findFile(scenePath);
    if (!resolved)
        return std::nullopt;

    return SlLib::Resources::Database::SlResourceDatabase{};
}

std::optional<std::filesystem::path> SlFile::findFile(std::string const& path)
{
    if (auto resolved = normalizePath(path))
        return resolved;

    for (auto const& root : s_gameDataFolders)
    {
        std::filesystem::path candidate = root / path;
        if (std::filesystem::exists(candidate))
            return std::filesystem::absolute(candidate);
    }

    return std::nullopt;
}
