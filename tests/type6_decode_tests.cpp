#include "SeEditor/Forest/ForestTypes.hpp"

#include <cmath>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool ReadFileToString(const char* path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    in.seekg(0, std::ios::end);
    std::streamoff size = in.tellg();
    if (size <= 0)
        return false;
    out.resize(static_cast<std::size_t>(size));
    in.seekg(0, std::ios::beg);
    in.read(out.data(), size);
    return in.good();
}

void AppendU16LE(std::vector<std::uint8_t>& data, std::uint16_t value)
{
    data.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    data.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
}

void AppendU32LE(std::vector<std::uint8_t>& data, std::uint32_t value)
{
    data.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    data.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    data.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    data.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

void AppendF32LE(std::vector<std::uint8_t>& data, float value)
{
    std::uint32_t raw = 0;
    static_assert(sizeof(raw) == sizeof(value), "float size mismatch");
    std::memcpy(&raw, &value, sizeof(raw));
    AppendU32LE(data, raw);
}

void AppendS16LE(std::vector<std::uint8_t>& data, std::int16_t value)
{
    std::uint16_t raw = static_cast<std::uint16_t>(value);
    AppendU16LE(data, raw);
}

bool NearlyEqual(float a, float b, float eps = 1.0e-3f)
{
    return std::fabs(a - b) <= eps;
}

bool TestElfFilesReadable()
{
    std::string cText;
    std::string hText;
    if (!ReadFileToString(SSR_WII_ELF_C_PATH, cText))
        return false;
    if (!ReadFileToString(SSR_WII_ELF_H_PATH, hText))
        return false;
    if (cText.find("__start") == std::string::npos)
        return false;
    if (hText.find("typedef unsigned char   undefined") == std::string::npos)
        return false;
    return true;
}

bool TestType6ParamAdvance()
{
    using SeEditor::Forest::SuAnimation;
    using SeEditor::Forest::SuRenderTree;

    std::vector<std::uint8_t> fullData;
    // Mask for one bone: translation + rotation (0x3).
    AppendU32LE(fullData, 0x00000003u);

    // Translation stream header: numFrames=2, numKeys=1, keyTimes=[0].
    AppendU16LE(fullData, 2);
    AppendU16LE(fullData, 1);
    AppendU16LE(fullData, 0);

    // Rotation stream header: numFrames=2, numKeys=1, keyTimes=[0].
    AppendU16LE(fullData, 2);
    AppendU16LE(fullData, 1);
    AppendU16LE(fullData, 0);

    const std::size_t streamBytes = fullData.size();
    const std::size_t paramOffset = streamBytes;

    // Translation params: min=0, delta=4, samples encode (1,2,3).
    AppendF32LE(fullData, 0.0f);
    AppendF32LE(fullData, 4.0f);
    const std::int16_t tRaw[3] = {8192, 16384, 24576};
    for (int set = 0; set < 3; ++set)
    {
        for (int c = 0; c < 3; ++c)
            AppendS16LE(fullData, tRaw[c]);
    }
    for (int c = 0; c < 3; ++c)
        AppendS16LE(fullData, tRaw[c]);

    // Rotation params: min=0, delta=1, samples encode (0,0,0,1).
    AppendF32LE(fullData, 0.0f);
    AppendF32LE(fullData, 1.0f);
    const std::int16_t rRaw[4] = {0, 0, 0, 32767};
    for (int set = 0; set < 3; ++set)
    {
        for (int c = 0; c < 4; ++c)
            AppendS16LE(fullData, rRaw[c]);
    }
    for (int c = 0; c < 4; ++c)
        AppendS16LE(fullData, rRaw[c]);

    SuAnimation anim;
    anim.Type = 0x06;
    anim.NumFrames = 2;
    anim.NumBones = 1;
    anim.Type6BigEndian = false;
    anim.Type6Block.assign(fullData.begin(), fullData.begin() + static_cast<std::ptrdiff_t>(streamBytes));
    anim.Type6DataPtr = fullData.data();
    anim.Type6DataSize = fullData.size();
    anim.Type6ParamDataOffset = static_cast<int>(paramOffset);
    anim.Type6Anchor = fullData.size() + 0x10000u;
    anim.Type6BlockStart = 0;
    anim.Type6BlockEnd = anim.Type6Block.size();

    SuRenderTree tree;
    tree.Translations.resize(1);
    tree.Rotations.resize(1);
    tree.Scales.resize(1);
    tree.Translations[0] = {0.0f, 0.0f, 0.0f, 0.0f};
    tree.Rotations[0] = {0.0f, 0.0f, 0.0f, 1.0f};
    tree.Scales[0] = {1.0f, 1.0f, 1.0f, 1.0f};

    if (!anim.DecodeType6Samples(tree))
        return false;

    for (int frame = 0; frame < 2; ++frame)
    {
        auto sample = anim.GetSample(frame, 0);
        if (!sample)
            return false;
        if (!NearlyEqual(sample->Translation.X, 1.0f) ||
            !NearlyEqual(sample->Translation.Y, 2.0f) ||
            !NearlyEqual(sample->Translation.Z, 3.0f))
        {
            return false;
        }
        if (!NearlyEqual(sample->Rotation.X, 0.0f) ||
            !NearlyEqual(sample->Rotation.Y, 0.0f) ||
            !NearlyEqual(sample->Rotation.Z, 0.0f) ||
            !NearlyEqual(sample->Rotation.W, 1.0f, 2.0e-3f))
        {
            return false;
        }
    }

    return true;
}

} // namespace

int main()
{
    int failures = 0;

    if (!TestElfFilesReadable())
    {
        std::cerr << "[FAIL] TestElfFilesReadable" << std::endl;
        ++failures;
    }
    else
    {
        std::cout << "[PASS] TestElfFilesReadable" << std::endl;
    }

    if (!TestType6ParamAdvance())
    {
        std::cerr << "[FAIL] TestType6ParamAdvance" << std::endl;
        ++failures;
    }
    else
    {
        std::cout << "[PASS] TestType6ParamAdvance" << std::endl;
    }

    if (failures != 0)
        return 1;

    return 0;
}
