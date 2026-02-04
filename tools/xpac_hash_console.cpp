#include <cstdint>
#include <cstdlib>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

static std::uint32_t XpacHashLittleEndianApproach(std::string_view text)
{
    std::uint32_t hash = 0;
    for (std::size_t i = text.size(); i-- > 0;)
    {
        hash = static_cast<std::uint32_t>(static_cast<unsigned char>(text[i]) + 131u * hash);
    }
    return hash;
}

static std::uint32_t XpacHashBigEndianApproach(std::string_view text)
{
    std::uint32_t hash = 0;
    for (char c : text)
    {
        hash = static_cast<std::uint32_t>(static_cast<unsigned char>(c) + 131u * hash);
    }
    return hash;
}

static std::uint32_t ByteSwap32(std::uint32_t value)
{
    return ((value & 0x000000FFu) << 24) |
           ((value & 0x0000FF00u) << 8) |
           ((value & 0x00FF0000u) >> 8) |
           ((value & 0xFF000000u) >> 24);
}

static std::string NormalizeForXpacHash(std::string text)
{
    for (char& c : text)
    {
        if (c == '/')
        {
            c = '\\';
            continue;
        }
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return text;
}

static void PrintValue(const char* label, std::uint32_t value)
{
    std::cout << label << ": 0x"
              << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << value
              << std::dec << std::setfill(' ') << " (" << value << ")\n";
}

int main()
{
    while (1)
    {
        std::system("cls");

        std::cout << "Input text (empty line exits): ";
        std::string input;
        if (!std::getline(std::cin, input) || input.empty())
            return 0;

        std::string normalized = NormalizeForXpacHash(input);
        std::uint32_t littleApproach = XpacHashLittleEndianApproach(normalized);
        std::uint32_t bigApproach = XpacHashBigEndianApproach(normalized);

        std::cout << "\n";
        std::cout << "Normalized: " << normalized << "\n";
        PrintValue("LE approach -> LE bytes", littleApproach);
        PrintValue("LE approach -> BE bytes", ByteSwap32(littleApproach));
        PrintValue("BE approach -> LE bytes", bigApproach);
        PrintValue("BE approach -> BE bytes", ByteSwap32(bigApproach));
        std::cout << "\n(Enter another input to continue...)\n";

        std::string discard;
        std::getline(std::cin, discard);
    }
}
