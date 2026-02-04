#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

static uint32_t XpacHash(const std::string& s)
{
    uint32_t hash = 0;
    for (std::size_t i = s.size(); i-- > 0;)
        hash = static_cast<uint32_t>(static_cast<unsigned char>(s[i]) + 131u * hash);
    return hash;
}

int main(int argc, char** argv)
{
    unsigned int threads = 16;
    if (argc >= 2) threads = std::max(1, std::stoi(argv[1]));

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
    std::atomic<uint64_t> hashes{0};

    auto start = std::chrono::steady_clock::now();

    auto worker = [&]() {
        std::string name(L, 'a');
        std::string base;
        base.reserve(prefix.size() + L + 32);
        for (;;) {
            uint64_t i = index.fetch_add(1);
            if (i >= total) break;
            uint64_t v = i;
            for (int pos = 0; pos < L; ++pos) {
                name[pos] = alphabet[static_cast<std::size_t>(v % alphabet.size())];
                v /= alphabet.size();
            }
            base = prefix + name;
            for (auto& c : base) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            std::replace(base.begin(), base.end(), '/', '\\');
            for (const auto& suf : suffixes) {
                std::string full = base + suf;
                (void)XpacHash(full);
                hashes.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (unsigned int i = 0; i < threads; ++i) pool.emplace_back(worker);
    for (auto& t : pool) t.join();

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Candidates: " << total << "\n";
    std::cout << "Suffixes per candidate: " << suffixes.size() << "\n";
    std::cout << "Total hashes: " << hashes.load() << "\n";
    std::cout << "Elapsed: " << elapsed.count() << " sec\n";
    std::cout << "Hash rate: " << (hashes.load() / elapsed.count()) << " hashes/sec\n";
    return 0;
}
