#include "ExcelPropertyNameLookup.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace SlLib::Lookup {

namespace {

void skipWhitespace(std::string const& text, std::size_t& pos)
{
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
        ++pos;
    }
}

std::string parseString(std::string const& text, std::size_t& pos)
{
    if (text[pos] != '"') {
        throw std::runtime_error("Invalid JSON string literal in ExcelPropertyNameLookup.");
    }

    ++pos;
    std::string value;
    while (pos < text.size()) {
        char c = text[pos++];
        if (c == '"') {
            break;
        }
        if (c == '\\' && pos < text.size()) {
            char escaped = text[pos++];
            value.push_back(escaped);
            continue;
        }
        value.push_back(c);
    }

    return value;
}

std::unordered_map<std::uint32_t, std::string> buildLookup()
{
    std::filesystem::path path = "Data/excel.lookup.json";
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Missing excel lookup JSON at " + path.string());
    }

    std::string content((std::istreambuf_iterator<char>(stream)), {});
    std::unordered_map<std::uint32_t, std::string> result;
    std::size_t pos = 0;

    skipWhitespace(content, pos);
    if (pos >= content.size() || content[pos] != '{') {
        throw std::runtime_error("Invalid JSON format for ExcelPropertyNameLookup.");
    }

    ++pos;
    while (true) {
        skipWhitespace(content, pos);
        if (pos >= content.size()) {
            break;
        }

        if (content[pos] == '}') {
            ++pos;
            break;
        }

        std::string key = parseString(content, pos);
        skipWhitespace(content, pos);
        if (pos >= content.size() || content[pos] != ':') {
            throw std::runtime_error("Invalid JSON separator in ExcelPropertyNameLookup.");
        }

        ++pos;
        skipWhitespace(content, pos);
        std::string value = parseString(content, pos);
        skipWhitespace(content, pos);

        uint32_t hash = 0;
        for (char c : key) {
            hash = hash * 10 + static_cast<unsigned char>(c - '0');
        }

        result[hash] = value;

        if (pos < content.size() && content[pos] == ',') {
            ++pos;
            continue;
        }
    }

    return result;
}

} // namespace

std::optional<std::string> ExcelPropertyNameLookup::GetPropertyName(std::uint32_t hash)
{
    const auto& lookup = Lookup();
    auto it = lookup.find(hash);
    if (it == lookup.end()) {
        return std::nullopt;
    }

    return it->second;
}

const std::unordered_map<std::uint32_t, std::string>& ExcelPropertyNameLookup::Lookup()
{
    static const auto lookup = buildLookup();
    return lookup;
}

} // namespace SlLib::Lookup
