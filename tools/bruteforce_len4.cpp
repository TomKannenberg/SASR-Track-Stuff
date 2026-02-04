#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

static uint32_t XpacHash(std::string_view s)
{
    uint32_t hash = 0;
    for (std::size_t i = s.size(); i-- > 0;)
        hash = static_cast<uint32_t>(static_cast<unsigned char>(s[i]) + 131u * hash);
    return hash;
}

static std::string ToUpper(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    return out;
}

static bool ParseHashFromFilename(const fs::path& path, uint32_t& out)
{
    auto stem = path.stem().string();
    if (stem.rfind("hash_", 0) != 0)
        return false;
    std::string hex = stem.substr(5);
    if (hex.size() != 8)
        return false;
    try
    {
        out = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: bruteforce_len4 <unknown_folder> [threads]\n";
        return 1;
    }

    fs::path unknownDir = argv[1];
    unsigned int threads = 16;
    if (argc >= 3)
        threads = std::max(1, std::stoi(argv[2]));

    std::unordered_set<uint32_t> targets;
    for (auto const& entry : fs::directory_iterator(unknownDir))
    {
        if (!entry.is_regular_file())
            continue;
        uint32_t hash = 0;
        if (ParseHashFromFilename(entry.path(), hash))
            targets.insert(hash);
    }

    if (targets.empty())
    {
        std::cerr << "No hash_XXXXXXXX files found in " << unknownDir << "\n";
        return 1;
    }

    const std::string alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const int L = 4;

    std::vector<std::string> suffixes;
    suffixes.reserve(14);
    suffixes.push_back(".zif");
    suffixes.push_back(".zig");
    for (const char* diff : {"Easy", "Medium", "Hard"})
    {
        suffixes.push_back(std::string("_") + diff + ".zif");
        suffixes.push_back(std::string("_") + diff + ".zig");
        suffixes.push_back(std::string("_") + diff + "_PCRT_SH_Data.zif");
        suffixes.push_back(std::string("_") + diff + "_PCRT_SH_Data.zig");
    }

    const std::string prefix = ".\\Resource\\Tracks\\";
    const uint64_t total = 1ULL * alphabet.size() * alphabet.size() * alphabet.size() * alphabet.size();

    std::atomic<uint64_t> index{0};
    std::atomic<uint64_t> matches{0};
    std::mutex outMutex;

    auto worker = [&]() {
        std::string name(L, 'a');
        std::string base;
        base.reserve(prefix.size() + L + 64);
        for (;;)
        {
            uint64_t i = index.fetch_add(1);
            if (i >= total)
                break;
            uint64_t v = i;
            for (int pos = 0; pos < L; ++pos)
            {
                name[pos] = alphabet[static_cast<std::size_t>(v % alphabet.size())];
                v /= alphabet.size();
            }

            base = prefix + name;
            std::string baseUpper = ToUpper(base);
            std::replace(baseUpper.begin(), baseUpper.end(), '/', '\\');

            for (const auto& suf : suffixes)
            {
                std::string full = baseUpper + suf;
                uint32_t h = XpacHash(full);
                if (targets.find(h) != targets.end())
                {
                    std::lock_guard<std::mutex> lock(outMutex);
                    std::cout << h << ":" << base << suf << ";" << "\n";
                    matches.fetch_add(1);
                }
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (unsigned int i = 0; i < threads; ++i)
        pool.emplace_back(worker);
    for (auto& t : pool)
        t.join();

    std::cerr << "Matches: " << matches.load() << "\n";
    return 0;
}
