#include <algorithm>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

static constexpr const char* kEndMarker = "1254590908:.\\__END__OF__FILE__;";

static uint32_t XpacHash(std::string_view s)
{
    uint32_t hash = 0;
    for (std::size_t i = s.size(); i-- > 0;)
    {
        hash = static_cast<uint32_t>(static_cast<unsigned char>(s[i]) + 131u * hash);
    }
    return hash;
}

static std::string ToLower(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

static std::string ToUpper(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    return out;
}

static std::vector<std::string> ReadLines(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::vector<std::string> lines;
    if (!in)
        return lines;
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

static bool WriteLines(const fs::path& path, const std::vector<std::string>& lines)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    for (const auto& line : lines)
        out << line << "\n";
    return true;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: ps3_mapping_add <MAPPING.GC> <PS3 resource root> [threads]\n";
        return 1;
    }

    fs::path mappingPath = argv[1];
    fs::path resourceRoot = argv[2];
    unsigned int threads = 16;
    if (argc >= 4)
        threads = std::max(1, std::stoi(argv[3]));

    auto lines = ReadLines(mappingPath);
    if (lines.empty())
    {
        std::cerr << "Failed to read mapping: " << mappingPath << "\n";
        return 1;
    }

    std::vector<std::string> kept;
    kept.reserve(lines.size());
    std::unordered_set<std::string> existing;
    existing.reserve(lines.size() * 2);

    for (auto& line : lines)
    {
        if (line.rfind("1254590908:", 0) == 0)
            continue;
        kept.push_back(line);
        auto colon = line.find(':');
        auto semi = line.find(';', colon == std::string::npos ? 0 : colon + 1);
        if (colon != std::string::npos && semi != std::string::npos)
        {
            std::string h = line.substr(0, colon);
            std::string v = line.substr(colon + 1, semi - colon - 1);
            std::string key = h + "|" + ToLower(v);
            existing.insert(std::move(key));
        }
    }

    std::vector<fs::path> files;
    files.reserve(50000);
    for (auto const& entry : fs::recursive_directory_iterator(resourceRoot))
    {
        if (!entry.is_regular_file())
            continue;
        files.push_back(entry.path());
    }

    std::atomic<std::size_t> index{0};
    std::mutex setMutex;
    std::mutex outMutex;
    std::vector<std::string> newLines;
    newLines.reserve(files.size());

    auto worker = [&]() {
        std::vector<std::string> localLines;
        localLines.reserve(1024);
        for (;;)
        {
            std::size_t i = index.fetch_add(1);
            if (i >= files.size())
                break;
            const auto& path = files[i];
            auto rel = path.lexically_relative(resourceRoot).generic_string();
            if (!rel.empty() && (rel.front() == '/' || rel.front() == '\\'))
                rel.erase(rel.begin());
            std::string value = ".\\Resource/" + rel;
            std::string hashInput = ToUpper(value);
            std::replace(hashInput.begin(), hashInput.end(), '/', '\\');
            uint32_t hash = XpacHash(hashInput);

            std::string key = std::to_string(hash) + "|" + ToLower(value);
            {
                std::lock_guard<std::mutex> lock(setMutex);
                if (existing.find(key) != existing.end())
                    continue;
                existing.insert(key);
            }

            localLines.push_back(std::to_string(hash) + ":" + value + ";");
            if (localLines.size() >= 2048)
            {
                std::lock_guard<std::mutex> lock(outMutex);
                newLines.insert(newLines.end(), localLines.begin(), localLines.end());
                localLines.clear();
            }
        }

        if (!localLines.empty())
        {
            std::lock_guard<std::mutex> lock(outMutex);
            newLines.insert(newLines.end(), localLines.begin(), localLines.end());
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (unsigned int i = 0; i < threads; ++i)
        pool.emplace_back(worker);
    for (auto& t : pool)
        t.join();

    if (!newLines.empty())
    {
        kept.insert(kept.end(), newLines.begin(), newLines.end());
    }
    kept.push_back(kEndMarker);

    if (!WriteLines(mappingPath, kept))
    {
        std::cerr << "Failed to write mapping: " << mappingPath << "\n";
        return 1;
    }

    std::cout << "Added " << newLines.size() << " new mappings." << std::endl;
    return 0;
}
