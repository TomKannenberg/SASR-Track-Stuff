#include "CharmyBee.hpp"
#include "SifParser.hpp"

#include "Editor/Panel/IEditorPanel.hpp"
#include "Editor/SceneManager.hpp"
#include "Editor/Scene.hpp"
#include "Editor/Selection.hpp"
#include "Dialogs/TinyFileDialog.hpp"
#include "Graphics/ImGui/ImGuiController.hpp"
#include "Installers/SlModelInstaller.hpp"
#include "Installers/SlTextureInstaller.hpp"
#include "Managers/SlFile.hpp"
#include "Renderer/SlRenderer.hpp"
#include "SlLib/Resources/Database/SlPlatform.hpp"

#include <SlLib/Excel/ExcelData.hpp>
#include <SlLib/Resources/Scene/SeDefinitionNode.hpp>
#include <SlLib/SumoTool/Siff/NavData/NavWaypoint.hpp>
#include <SlLib/SumoTool/Siff/Navigation.hpp>

#include <imgui.h>

#include <limits>
#include <functional>
#include <array>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <chrono>
#include <fstream>
#include <functional>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <cstdint>
#include <cstring>
#include <bit>
#include <numbers>
#include <zlib.h>

namespace {

using SeEditor::SifChunkInfo;

std::string DescribeGlfwKey(int key)
{
    if (auto name = glfwGetKeyName(key, 0))
        return std::string(name);

    switch (key)
    {
    case GLFW_KEY_SPACE: return "Space";
    case GLFW_KEY_ESCAPE: return "Escape";
    case GLFW_KEY_ENTER: return "Enter";
    case GLFW_KEY_TAB: return "Tab";
    case GLFW_KEY_BACKSPACE: return "Backspace";
    case GLFW_KEY_INSERT: return "Insert";
    case GLFW_KEY_DELETE: return "Delete";
    case GLFW_KEY_RIGHT: return "Right";
    case GLFW_KEY_LEFT: return "Left";
    case GLFW_KEY_DOWN: return "Down";
    case GLFW_KEY_UP: return "Up";
    case GLFW_KEY_PAGE_UP: return "PageUp";
    case GLFW_KEY_PAGE_DOWN: return "PageDown";
    case GLFW_KEY_HOME: return "Home";
    case GLFW_KEY_END: return "End";
    case GLFW_KEY_CAPS_LOCK: return "CapsLock";
    case GLFW_KEY_SCROLL_LOCK: return "ScrollLock";
    case GLFW_KEY_NUM_LOCK: return "NumLock";
    case GLFW_KEY_PRINT_SCREEN: return "PrintScreen";
    case GLFW_KEY_PAUSE: return "Pause";
    case GLFW_KEY_F1: return "F1";
    case GLFW_KEY_F2: return "F2";
    case GLFW_KEY_F3: return "F3";
    case GLFW_KEY_F4: return "F4";
    case GLFW_KEY_F5: return "F5";
    case GLFW_KEY_F6: return "F6";
    case GLFW_KEY_F7: return "F7";
    case GLFW_KEY_F8: return "F8";
    case GLFW_KEY_F9: return "F9";
    case GLFW_KEY_F10: return "F10";
    case GLFW_KEY_F11: return "F11";
    case GLFW_KEY_F12: return "F12";
    default:
        return "Key-" + std::to_string(key);
    }
}

void ReportSifError(std::string const& message)
{
    std::cerr << "[CharmyBee][SIF] " << message << std::endl;
}

void PrintSceneInfo()
{
    auto* scene = SeEditor::Editor::SceneManager::Current();
    if (scene == nullptr)
    {
        std::cerr << "[CharmyBee] No active scene registered." << std::endl;
        return;
    }

    std::cout << "[CharmyBee] Active scene: " << scene->SourceFileName << std::endl;
    std::cout << "[CharmyBee] Scene definitions: " << scene->Database.RootDefinitions.size() << std::endl;
}

std::optional<std::pair<int, int>> ParseWaypointRoute(std::string const& name)
{
    constexpr std::string_view prefix = "waypoint_";
    if (name.rfind(prefix, 0) != 0)
        return std::nullopt;

    std::string rest = name.substr(prefix.size());
    auto delim = rest.find('_');
    if (delim == std::string::npos)
        return std::nullopt;

    try
    {
        int routeId = std::stoi(rest.substr(0, delim));
        int pointId = std::stoi(rest.substr(delim + 1));
        return std::make_pair(routeId, pointId);
    }
    catch (...)
    {
        return std::nullopt;
    }
}


std::vector<std::string> FormatHexDump(std::vector<std::uint8_t> const& data)
{
    constexpr char hexDigits[] = "0123456789ABCDEF";
    std::vector<std::string> lines;
    size_t totalLines = (data.size() + 15) / 16;
    lines.reserve(std::max<size_t>(1, totalLines));

    for (size_t offset = 0; offset < data.size(); offset += 16)
    {
        char header[32];
        std::snprintf(header, sizeof(header), "%08llX: ", static_cast<unsigned long long>(offset));
        std::string line = header;

        for (size_t column = 0; column < 16; ++column)
        {
            if (offset + column < data.size())
            {
                auto byte = data[offset + column];
                line.push_back(hexDigits[(byte >> 4) & 0xF]);
                line.push_back(hexDigits[byte & 0xF]);
                line.push_back(' ');
            }
            else
            {
                line.append("   ");
            }
        }

        line.append(" | ");
        for (size_t column = 0; column < 16; ++column)
        {
            if (offset + column < data.size())
            {
                auto byte = data[offset + column];
                line.push_back(std::isprint(byte) ? static_cast<char>(byte) : '.');
            }
            else
            {
                line.push_back(' ');
            }
        }

        lines.push_back(std::move(line));
    }

    if (lines.empty())
        lines.push_back("File is empty.");

    return lines;
}

constexpr std::uint32_t MakeTypeCode(char a, char b, char c, char d)
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(a)) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 8) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(d)) << 24);
}

constexpr int kZlibFlushNone = 0;
constexpr int kZlibOkValue = 0;
constexpr int kZlibStreamEndValue = 1;

enum class SifResourceType : std::uint32_t
{
    Info = 0x4F464E49,
    TexturePack = 0x58455450,
    KeyFrameLibrary = 0x4D52464B,
    ObjectDefLibrary = 0x534A424F,
    SceneLibrary = 0x454E4353,
    FontPack = 0x544E4F46,
    TextPack = 0x54584554,
    Relocations = 0x4F4C4552,
    Forest = 0x45524F46,
    Navigation = 0x4B415254,
    Trail = 0x4C415254,
    Logic = 0x43474F4C,
    VisData = 0x34445650,
    ShData = 0x31304853,
    LensFlare2 = 0x3220464C,
    LensFlare1 = 0x3120464C,
    Collision = 0x494C4F43,
};

std::string ResourceTypeName(std::uint32_t value)
{
    switch (static_cast<SifResourceType>(value))
    {
    case SifResourceType::Info:
        return "Info";
    case SifResourceType::TexturePack:
        return "TexturePack";
    case SifResourceType::KeyFrameLibrary:
        return "KeyFrameLibrary";
    case SifResourceType::ObjectDefLibrary:
        return "ObjectDefLibrary";
    case SifResourceType::SceneLibrary:
        return "SceneLibrary";
    case SifResourceType::FontPack:
        return "FontPack";
    case SifResourceType::TextPack:
        return "TextPack";
    case SifResourceType::Forest:
        return "Forest";
    case SifResourceType::Navigation:
        return "Navigation";
    case SifResourceType::Trail:
        return "Trail";
    case SifResourceType::Logic:
        return "Logic";
    case SifResourceType::VisData:
        return "VisData";
    case SifResourceType::ShData:
        return "ShData";
    case SifResourceType::LensFlare2:
        return "LensFlare2";
    case SifResourceType::LensFlare1:
        return "LensFlare1";
    case SifResourceType::Collision:
        return "Collision";
    default:
        return "Unknown";
    }
}

std::uint32_t ReadUint32(const std::uint8_t* ptr)
{
    return static_cast<std::uint32_t>(ptr[0]) |
           (static_cast<std::uint32_t>(ptr[1]) << 8) |
           (static_cast<std::uint32_t>(ptr[2]) << 16) |
           (static_cast<std::uint32_t>(ptr[3]) << 24);
}

std::uint32_t ReadUint32BE(const std::uint8_t* ptr)
{
    return static_cast<std::uint32_t>(ptr[3]) |
           (static_cast<std::uint32_t>(ptr[2]) << 8) |
           (static_cast<std::uint32_t>(ptr[1]) << 16) |
           (static_cast<std::uint32_t>(ptr[0]) << 24);
}

std::uint16_t ReadUint16(const std::uint8_t* ptr)
{
    return static_cast<std::uint16_t>(ptr[0]) |
           (static_cast<std::uint16_t>(ptr[1]) << 8);
}

float HalfToFloat(std::uint16_t value)
{
    std::uint16_t sign = (value >> 15) & 1;
    std::uint16_t exp = (value >> 10) & 0x1F;
    std::uint16_t mant = value & 0x3FF;

    if (exp == 0)
    {
        if (mant == 0)
            return sign ? -0.0f : 0.0f;
        float m = mant / 1024.0f;
        float val = std::ldexp(m, -14);
        return sign ? -val : val;
    }
    if (exp == 31)
        return mant ? std::numeric_limits<float>::quiet_NaN()
                    : (sign ? -std::numeric_limits<float>::infinity()
                            : std::numeric_limits<float>::infinity());

    float m = 1.0f + mant / 1024.0f;
    float val = std::ldexp(m, exp - 15);
    return sign ? -val : val;
}

float ReadFloatLE(const std::uint8_t* ptr)
{
    std::uint32_t v = ReadUint32(ptr);
    float out;
    std::memcpy(&out, &v, sizeof(float));
    return out;
}

float ReadFloatBE(const std::uint8_t* ptr)
{
    std::uint32_t v = ReadUint32BE(ptr);
    float out;
    std::memcpy(&out, &v, sizeof(float));
    return out;
}

bool ParseCollisionMeshChunk(SifChunkInfo const& chunk,
                             std::vector<SlLib::Math::Vector3>& vertices,
                             std::vector<std::array<int, 3>>& triangles,
                             std::string& error)
{
    using SlLib::Math::Vector3;
    auto const& data = chunk.Data;
    bool be = chunk.BigEndian;
    auto read32 = [&](std::size_t off) -> std::uint32_t {
        if (off + 4 > data.size())
            return 0;
        return be ? ReadUint32BE(data.data() + off) : ReadUint32(data.data() + off);
    };
    auto read16 = [&](std::size_t off) -> std::uint16_t {
        if (off + 2 > data.size())
            return 0;
        return be ? static_cast<std::uint16_t>((data[off] << 8) | data[off + 1])
                  : static_cast<std::uint16_t>(data[off] | (data[off + 1] << 8));
    };
    auto readFloat = [&](std::size_t off) -> float {
        if (off + 4 > data.size())
            return 0.0f;
        return be ? ReadFloatBE(data.data() + off) : ReadFloatLE(data.data() + off);
    };

    if (data.size() < 0x48)
    {
        error = "Collision chunk too small.";
        return false;
    }

    std::uint32_t numVertices = read32(0x8);
    std::uint32_t numTriangles = read32(0xC);

    std::uint32_t verticesPtr = read32(0x30);
    std::uint32_t trianglesPtr = read32(0x34);

    if (verticesPtr == 0 || trianglesPtr == 0)
    {
        error = "Collision chunk missing vertex/triangle pointers.";
        return false;
    }

    if (verticesPtr + numVertices * 0x10 > data.size())
    {
        error = "Collision vertices outside chunk bounds.";
        return false;
    }
    if (trianglesPtr + numTriangles * 0x0C > data.size())
    {
        error = "Collision triangles outside chunk bounds.";
        return false;
    }

    vertices.clear();
    vertices.reserve(numVertices);
    for (std::size_t i = 0; i < numVertices; ++i)
    {
        std::size_t off = verticesPtr + i * 0x10;
        Vector3 v{
            readFloat(off + 0),
            readFloat(off + 4),
            readFloat(off + 8)
        };
        vertices.push_back(v);
    }

    triangles.clear();
    triangles.reserve(numTriangles);
    for (std::size_t i = 0; i < numTriangles; ++i)
    {
        std::size_t off = trianglesPtr + i * 0x0C;
        std::uint16_t v0 = read16(off + 0);
        std::uint16_t v1 = read16(off + 2);
        std::uint16_t v2 = read16(off + 4);
        if (v0 >= vertices.size() || v1 >= vertices.size() || v2 >= vertices.size())
            continue;
        triangles.push_back({static_cast<int>(v0), static_cast<int>(v1), static_cast<int>(v2)});
    }

    return true;
}

bool LooksLikeZlib(std::span<const std::uint8_t> data)
{
    if (data.size() < 2)
        return false;

    std::uint8_t cmf = data[0];
    std::uint8_t flg = data[1];
    if ((cmf & 0x0F) != 8)
        return false;

    return (((static_cast<int>(cmf) << 8) | flg) % 31) == 0;
}

std::vector<std::uint8_t> DecompressZlib(std::span<const std::uint8_t> stream)
{
    z_stream inflater{};
    inflater.next_in = const_cast<std::uint8_t*>(stream.data());
    inflater.avail_in = static_cast<decltype(inflater.avail_in)>(stream.size());

    if (inflateInit(&inflater) != kZlibOkValue)
        throw std::runtime_error("Failed to init zlib inflater.");

    std::vector<std::uint8_t> result;
    std::vector<std::uint8_t> buffer(1 << 14);

    int status = kZlibOkValue;
    while (status != kZlibStreamEndValue)
    {
        inflater.next_out = buffer.data();
        inflater.avail_out = static_cast<decltype(inflater.avail_out)>(buffer.size());

        status = inflate(&inflater, kZlibFlushNone);
        if (status != kZlibOkValue && status != kZlibStreamEndValue)
        {
            inflateEnd(&inflater);
            throw std::runtime_error("SIF decompression failure.");
        }

        size_t have = buffer.size() - inflater.avail_out;
        result.insert(result.end(), buffer.begin(), buffer.begin() + have);
    }

    inflateEnd(&inflater);
    return result;
}

std::uint32_t ReadUint32LE(const std::uint8_t* ptr)
{
    return static_cast<std::uint32_t>(ptr[0]) |
           (static_cast<std::uint32_t>(ptr[1]) << 8) |
           (static_cast<std::uint32_t>(ptr[2]) << 16) |
           (static_cast<std::uint32_t>(ptr[3]) << 24);
}

void StripLengthPrefixIfPresent(std::vector<std::uint8_t>& data)
{
    if (data.size() < 4)
        return;

    std::size_t size = data.size();
    std::uint32_t le = ReadUint32LE(data.data());
    std::uint32_t be = ReadUint32BE(data.data());
    if (le == size - 4 || le == size || be == size - 4 || be == size)
        data.erase(data.begin(), data.begin() + 4);
}

struct SifParseResult
{
    bool WasCompressed = false;
    std::size_t DecompressedSize = 0;
    std::vector<SifChunkInfo> Chunks;
};

std::string FormatRelocationList(std::vector<std::uint32_t> const& relocations)
{
    if (relocations.empty())
        return {};

    std::ostringstream builder;
    builder << "  Relocations:";
    std::size_t limit = std::min<std::size_t>(relocations.size(), 6);
    for (std::size_t i = 0; i < limit; ++i)
    {
        builder << " 0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
                << relocations[i];
    }

    if (relocations.size() > limit)
        builder << " ...";

    return builder.str();
}

std::string QuoteArgument(std::string const& value)
{
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (char ch : value)
    {
        if (ch == '"')
            escaped.append("\\\"");
        else
            escaped.push_back(ch);
    }
    escaped.push_back('"');
    return escaped;
}

} // namespace

namespace SeEditor {

CharmyBee::CharmyBee(std::string title, int width, int height, bool debugKeyInput)
    : _title(std::move(title))
    , _width(width)
    , _height(height)
    , _debugKeyInput(debugKeyInput)
{
    ResetAssetTree();
}

CharmyBee::~CharmyBee() = default;

CharmyBee::TreeNode::TreeNode(std::string name, bool isFolder)
    : Name(std::move(name))
    , IsFolder(isFolder)
{}

void CharmyBee::OnLoad()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.TabRounding = 1.0f;
    style.FrameRounding = 1.0f;
    style.ScrollbarRounding = 1.0f;
    style.WindowRounding = 0.0f;

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigDragClickToInputText = true;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    SlFile::AddGameDataFolder("F:/sart/game/pc");
    SlFile::AddGameDataFolder("C:/Program Files (x86)/Steam/steamapps/common/Sonic & All-Stars Racing Transformed/Data");

    if (auto racers = SlFile::GetExcelData("gamedata/racers"))
        _racerData = std::move(*racers);
    else
        std::cerr << "[CharmyBee] Unable to load racer data\n";

    if (auto tracks = SlFile::GetExcelData("gamedata/tracks"))
        _trackData = std::move(*tracks);
    else
        std::cerr << "[CharmyBee] Unable to load track data\n";

    _renderer.Initialize();

    if (_quickstart)
    {
        try
        {
            Editor::SceneManager::LoadScene("levels/seasidehill2/seasidehill2");
            SetupNavigationRendering();
            OnWorkspaceLoad();
        }
        catch (std::exception const& exception)
        {
            std::cerr << "[CharmyBee] Failed to load quickstart scene: " << exception.what() << std::endl;
        }
    }
}

void CharmyBee::SetupNavigationRendering()
{
    auto* scene = Editor::SceneManager::Current();
    if (scene == nullptr)
        return;

    _navigation = scene->Navigation;
    if (_navigation == nullptr)
        return;

    _navigationTool = std::make_unique<Editor::Tools::NavigationTool>(_navigation);

    struct RouteInfo
    {
        int Id;
        std::vector<std::pair<int, SlLib::SumoTool::Siff::NavData::NavWaypoint*>> Waypoints;
    };

    std::vector<RouteInfo> routeInfos;
    for (auto const& waypoint : _navigation->Waypoints)
    {
        if (waypoint == nullptr)
            continue;

        if (auto parsed = ParseWaypointRoute(waypoint->Name))
        {
            auto it = std::find_if(routeInfos.begin(), routeInfos.end(),
                [parsed](RouteInfo const& info) { return info.Id == parsed->first; });
            if (it == routeInfos.end())
            {
                routeInfos.push_back(RouteInfo{parsed->first, {}});
                it = std::prev(routeInfos.end());
            }

            it->Waypoints.emplace_back(parsed->second, waypoint.get());
        }
    }

    std::sort(routeInfos.begin(), routeInfos.end(), [](RouteInfo const& a, RouteInfo const& b) {
        return a.Id < b.Id;
    });

    _routes.clear();
    for (auto& info : routeInfos)
    {
        std::sort(info.Waypoints.begin(), info.Waypoints.end(),
            [](auto const& lhs, auto const& rhs) { return lhs.first < rhs.first; });

        Editor::Tools::NavTool::NavRoute route(info.Id);
        for (auto const& entry : info.Waypoints)
            route.Waypoints.push_back(entry.second);

        _routes.push_back(std::move(route));
    }

    if (!_routes.empty())
    {
        _selectedRoute = &_routes.front();
        if (!_selectedRoute->Waypoints.empty())
            _selectedWaypoint = _selectedRoute->Waypoints[0];
    }

    _selectedRacingLine = _navigation->RacingLines.empty() ? -1 : 0;
    _selectedRacingLineSegment =
        (_selectedRacingLine == -1 || _navigation->RacingLines[_selectedRacingLine].Segments.empty()) ? -1 : 0;

    std::cout << "[CharmyBee] Navigation rendering configured." << std::endl;
}

void CharmyBee::OnWorkspaceLoad()
{
    auto* scene = Editor::SceneManager::Current();
    if (scene == nullptr)
        return;

    _database = &scene->Database;
    if (_database == nullptr || Editor::SceneManager::DisableRendering)
        return;

    ResetAssetTree();

    std::string name = scene->SourceFileName;
    if (name.empty())
        name = "Unnamed Workspace";
    _title = "Sumo Engine Editor - " + name + " <OpenGL>";
    std::cout << "[CharmyBee] Workspace loaded: " << _title << std::endl;

    for (auto* definition : _database->RootDefinitions)
    {
        if (definition == nullptr)
            continue;

        std::string nodeName = definition->ShortName.empty() ? definition->UidName : definition->ShortName;
        AddItemNode(nodeName, definition);
    }
}

void CharmyBee::TriggerCloseWorkspace()
{
    _title = "Sumo Engine Editor - <OpenGL>";
    if (_database == nullptr)
    {
        _requestedWorkspaceClose = false;
        return;
    }

    std::cout << "[CharmyBee] Closing workspace..." << std::endl;
    _database = nullptr;
    _navigation = nullptr;
    _routes.clear();
    _selectedRoute = nullptr;
    _selectedWaypoint = nullptr;
    _selectedRacingLine = -1;
    _selectedRacingLineSegment = -1;

    ResetAssetTree();
    _requestedWorkspaceClose = false;
}

void CharmyBee::DrawNodeCreationMenu()
{
    if (ImGui::MenuItem("Create Empty Folder", "Ctrl+Shift+N"))
        std::cout << "[CharmyBee] Create Empty Folder requested." << std::endl;
}

void CharmyBee::ResetAssetTree()
{
    _assetRoot = std::make_unique<TreeNode>("assets", true);
    _selectedFolder = _assetRoot.get();
}

std::vector<std::string> CharmyBee::GetPathComponents(std::string const& value) const
{
    std::vector<std::string> parts;
    parts.reserve(8);

    std::string segment;
    segment.reserve(value.size());
    for (char ch : value)
    {
        if (ch == '/' || ch == '\\')
        {
            if (!segment.empty())
            {
                parts.push_back(segment);
                segment.clear();
            }
            continue;
        }

        segment.push_back(ch);
    }

    if (!segment.empty())
        parts.push_back(segment);

    return parts;
}

CharmyBee::TreeNode* CharmyBee::AddFolderNode(std::string const& path)
{
    if (_assetRoot == nullptr)
        ResetAssetTree();

    if (path.empty())
        return _assetRoot.get();

    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    while (!normalized.empty() && normalized.back() == '/')
        normalized.pop_back();

    TreeNode* parent = _assetRoot.get();
    for (auto const& component : GetPathComponents(normalized))
    {
        auto it = std::find_if(parent->Children.begin(), parent->Children.end(),
            [&](std::unique_ptr<TreeNode> const& child) {
                return child->IsFolder && child->Name == component;
            });

        if (it != parent->Children.end())
        {
            parent = it->get();
            continue;
        }

        auto folder = std::make_unique<TreeNode>(component, true);
        TreeNode* folderPtr = folder.get();
        parent->Children.push_back(std::move(folder));
        parent = folderPtr;
    }

    return parent;
}

void CharmyBee::AddItemNode(std::string const& name, SlLib::Resources::Scene::SeDefinitionNode* definition)
{
    if (name.empty())
        return;

    if (_assetRoot == nullptr)
        ResetAssetTree();

    std::string normalized = name;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    TreeNode* parent = _assetRoot.get();
    auto lastSlash = normalized.find_last_of('/');
    if (lastSlash != std::string::npos)
        parent = AddFolderNode(normalized.substr(0, lastSlash));

    std::string nodeName = lastSlash == std::string::npos ? normalized : normalized.substr(lastSlash + 1);
    if (nodeName.empty())
        nodeName = normalized;

    auto node = std::make_unique<TreeNode>(nodeName);
    node->Association = definition;
    parent->Children.push_back(std::move(node));

    std::cout << "[CharmyBee] Queued asset node: " << name << std::endl;
}

void CharmyBee::RenderWorkspace()
{
    if (_inspectorPanel)
        _inspectorPanel->OnSelectionChanged();

    ImGui::Begin("Inspector");
    if (_inspectorPanel)
        _inspectorPanel->OnImGuiRender();
    ImGui::End();

    ImGui::Begin("Assets");
    if (_assetPanel)
        _assetPanel->OnImGuiRender();
    RenderAssetView();
    ImGui::End();

    RenderSceneView();
    RenderRacingLineEditor();
}

void CharmyBee::RenderHierarchyWindow()
{
    if (!_showHierarchyWindow)
        return;

    if (!ImGui::Begin("Hierarchy", &_showHierarchyWindow))
    {
        ImGui::End();
        return;
    }

    if (_database == nullptr)
    {
        ImGui::Text("No workspace loaded.");
        ImGui::TextDisabled("The hierarchy panel opens automatically once a workspace is active.");
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("##hab_hierarchy_tabs"))
    {
        if (ImGui::BeginTabItem("Scene"))
        {
            RenderHierarchy(false);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Definitions"))
        {
            RenderHierarchy(true);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void CharmyBee::RenderHierarchy(bool definitions)
{
    if (_database == nullptr)
        return;

    std::function<void(SlLib::Resources::Scene::SeGraphNode*)> drawTree;
    drawTree = [&](SlLib::Resources::Scene::SeGraphNode* node) {
        if (node == nullptr)
            return;

        std::string label = node->ShortName.empty() ? node->UidName : node->ShortName;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (node->FirstChild == nullptr)
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (Editor::Selection::ActiveNode == node)
            flags |= ImGuiTreeNodeFlags_Selected;

        bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(node), flags, "%s", label.c_str());
        if (ImGui::IsItemClicked())
            Editor::Selection::ActiveNode = node;

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Delete"))
                _requestedNodeDeletion = node;
            ImGui::EndPopup();
        }

        if (open)
        {
            SlLib::Resources::Scene::SeGraphNode* child = node->FirstChild;
            while (child != nullptr)
            {
                drawTree(child);
                child = child->NextSibling;
            }

            ImGui::TreePop();
        }
    };

    if (definitions)
    {
        for (auto* definition : _database->RootDefinitions)
        {
            if (definition == nullptr)
                continue;

            drawTree(definition);
        }
    }
    else
    {
        SlLib::Resources::Scene::SeGraphNode* child = _database->Scene.FirstChild;
        while (child != nullptr)
        {
            drawTree(child);
            child = child->NextSibling;
        }
    }
}

void CharmyBee::RenderAssetView()
{
    if (_assetRoot == nullptr)
        return;

    ImGui::BeginChild("Folder View", ImVec2(150.0f, 0.0f), true);

    std::function<void(TreeNode*)> drawFolder;
    drawFolder = [&](TreeNode* folder) {
        if (folder == nullptr)
            return;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (folder->Children.empty())
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (_selectedFolder == folder)
            flags |= ImGuiTreeNodeFlags_Selected;

        bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(folder), flags, "%s", folder->Name.c_str());
        if (ImGui::IsItemClicked())
            _selectedFolder = folder;

        if (open)
        {
            for (auto& child : folder->Children)
            {
                if (child->IsFolder)
                    drawFolder(child.get());
            }

            ImGui::TreePop();
        }
    };

    drawFolder(_assetRoot.get());
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("Item View", ImVec2(0.0f, 0.0f), true);
    if (_selectedFolder != nullptr)
    {
        for (auto& child : _selectedFolder->Children)
        {
            if (child->IsFolder)
                continue;

            bool selected = child->Association != nullptr && Editor::Selection::ActiveNode == child->Association;
            if (ImGui::Selectable(child->Name.c_str(), selected))
            {
                if (child->Association != nullptr)
                    Editor::Selection::ActiveNode = child->Association;
            }
        }
    }
    ImGui::EndChild();
}

void CharmyBee::RenderSceneView()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Scene");

    if (_scenePanel)
        _scenePanel->OnImGuiRender();

    if (_debugKeyInput)
        PollGlfwKeyInput();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    _localSceneFrameWidth = avail.x;
    _localSceneFrameHeight = avail.y;

    ImGui::Dummy(avail);
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_WindowBg));
    ImGui::GetWindowDrawList()->AddText(ImVec2(min.x + 10.0f, min.y + 10.0f),
                                        ImGui::GetColorU32(ImGuiCol_TextDisabled),
                                        "Scene rendering placeholder");

    if (_navigationTool)
        _navigationTool->OnRender();

    // Orbit controls (right-drag rotate, scroll zoom) when hovering scene.
    ImGuiIO& io = ImGui::GetIO();
    float dt = io.DeltaTime > 0.0f ? io.DeltaTime : 1.0f / 60.0f;
    bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                                          ImGuiHoveredFlags_AllowWhenBlockedByPopup);
    if (hovered && io.MouseWheel != 0.0f)
    {
        _orbitDistance *= std::pow(0.9f, io.MouseWheel);
    }

    if (_debugKeyInput)
        PollGlfwKeyInput();

    ImGui::End();
    ImGui::PopStyleVar();
}

void CharmyBee::UpdateOrbitFromInput(float delta)
{
    if (_controller == nullptr)
        return;

    GLFWwindow* window = _controller->Window();
    if (window == nullptr)
        return;

    auto isDown = [&](int key) {
        int state = glfwGetKey(window, key);
        return state == GLFW_PRESS || state == GLFW_REPEAT;
    };

    float rotSpeed = 1.8f;
    float moveSpeed = _movementSpeed;

    if (isDown(GLFW_KEY_J)) _orbitYaw   -= rotSpeed * delta;
    if (isDown(GLFW_KEY_L)) _orbitYaw   += rotSpeed * delta;
    if (isDown(GLFW_KEY_I)) _orbitPitch -= rotSpeed * delta;
    if (isDown(GLFW_KEY_K)) _orbitPitch += rotSpeed * delta;

    if (isDown(GLFW_KEY_Z)) _movementSpeed /= 1.05f;
    if (isDown(GLFW_KEY_X)) _movementSpeed *= 1.05f;
    _movementSpeed = std::clamp(_movementSpeed, 1.0f, 40.0f);

    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos(window, &cursorX, &cursorY);
    SlLib::Math::Vector2 cursorPos{static_cast<float>(cursorX), static_cast<float>(cursorY)};
    bool windowFocused = glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;
    if (windowFocused)
    {
        auto isMouseDown = [&](int button) {
            int state = glfwGetMouseButton(window, button);
            return state == GLFW_PRESS || state == GLFW_REPEAT;
        };
        if (isMouseDown(GLFW_MOUSE_BUTTON_LEFT) || isMouseDown(GLFW_MOUSE_BUTTON_RIGHT))
        {
            SlLib::Math::Vector2 current = cursorPos;
            if (_mouseOrbitTracking)
            {
                SlLib::Math::Vector2 delta = {current.X - _mouseOrbitLastPos.X, current.Y - _mouseOrbitLastPos.Y};
                constexpr float mouseSensitivity = 0.01f;
                _orbitYaw   -= delta.X * mouseSensitivity;
                _orbitPitch -= delta.Y * mouseSensitivity;
                if (_debugKeyInput && (std::abs(delta.X) > 0.0f || std::abs(delta.Y) > 0.0f))
                {
                    std::cout << "[CharmyBee][MouseDrag] dx=" << delta.X << " dy=" << delta.Y << std::endl;
                }
            }
            else
            {
                _mouseOrbitTracking = true;
            }
            _mouseOrbitLastPos = current;
        }
        else
        {
            _mouseOrbitTracking = false;
        }
    }
    else
    {
        _mouseOrbitTracking = false;
    }

    float cp = std::cos(_orbitPitch);
    float sp = std::sin(_orbitPitch);
    float cy = std::cos(_orbitYaw);
    float sy = std::sin(_orbitYaw);

    // Camera-forward points toward the target; right is orthogonal in camera space.
    SlLib::Math::Vector3 forward{-cp * cy, -sp, -cp * sy};
    SlLib::Math::Vector3 right{sy, 0.0f, -cy};

    SlLib::Math::Vector3 deltaVec{0.0f, 0.0f, 0.0f};
    if (isDown(GLFW_KEY_W)) deltaVec = deltaVec + forward;
    if (isDown(GLFW_KEY_S)) deltaVec = deltaVec - forward;
    if (isDown(GLFW_KEY_A)) deltaVec = deltaVec - right;
    if (isDown(GLFW_KEY_D)) deltaVec = deltaVec + right;

        if (deltaVec.X != 0.0f || deltaVec.Y != 0.0f || deltaVec.Z != 0.0f)
        {
            float len = SlLib::Math::length(deltaVec);
            if (len > 0.0f)
                deltaVec = deltaVec * (1.0f / len);
            deltaVec = deltaVec * (moveSpeed * delta * std::max(1.0f, _orbitDistance * 0.2f));
            _orbitOffset = _orbitOffset + deltaVec;
        }

    _orbitPitch = std::clamp(_orbitPitch, -1.4f, 1.4f);
    _orbitDistance = std::max(0.3f, _orbitDistance);

    _renderer.SetOrbitCamera(_orbitYaw, _orbitPitch, _orbitDistance, _orbitTarget + _orbitOffset);
    _renderer.SetDrawCollisionMesh(_drawCollisionMesh);
}

void CharmyBee::RenderRacingLineEditor()
{
    ImGui::Begin("Navigation");

    if (_navigation == nullptr)
    {
        ImGui::TextDisabled("Navigation data is missing.");
    }
    else
    {
        ImGui::Text("Routes: %zu", _routes.size());
        ImGui::Text("Waypoints: %zu", _navigation->Waypoints.size());
        ImGui::Text("Racing lines: %zu", _navigation->RacingLines.size());
        if (_selectedRoute != nullptr)
            ImGui::Text("Selected route: %d (%zu waypoints)", _selectedRoute->Id,
                        _selectedRoute->Waypoints.size());
        else
            ImGui::Text("Selected route: none");

        ImGui::Text("Selected racing line: %d", _selectedRacingLine);
    }

    ImGui::End();
}

void CharmyBee::RenderProjectSelector()
{
    ImGui::Begin("Projects");
    ImGui::Text("No workspace loaded.");
    ImGui::TextDisabled("Quickstart loads seasidehill2 automatically.");
    ImGui::End();
}

void CharmyBee::RenderSifViewer()
{
    if (!ImGui::Begin("SIF Viewer"))
    {
        ImGui::End();
        return;
    }

    if (_sifFilePath.empty())
    {
        ImGui::TextDisabled("No SIF/ZIF/SIG/ZIG file loaded. Use File > Load SIF/ZIF/SIG/ZIG File...");
    }
    else
    {
        ImGui::TextWrapped("File: %s", _sifFilePath.c_str());
        if (!_sifLoadMessage.empty())
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "%s", _sifLoadMessage.c_str());

        ImGui::Text("Original size: %llu bytes", static_cast<unsigned long long>(_sifOriginalSize));
        if (_sifFileCompressed)
            ImGui::Text("Decompressed payload: %llu bytes", static_cast<unsigned long long>(_sifDecompressedSize));

        ImGui::Separator();
        if (!_sifChunks.empty())
        {
            ImGui::Text("Chunks: %zu", _sifChunks.size());
            ImGui::BeginChild("chunkList", ImVec2(ImGui::GetContentRegionAvail().x * 0.45f, 200.0f), true);
            for (std::size_t i = 0; i < _sifChunks.size(); ++i)
            {
                bool selected = static_cast<int>(i) == _selectedChunk;
                char label[128];
                std::snprintf(label, sizeof(label), "[%02zu] %s (0x%08X)", i, _sifChunks[i].Name.c_str(),
                              _sifChunks[i].TypeValue);
                if (ImGui::Selectable(label, selected))
                    _selectedChunk = static_cast<int>(i);
            }
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("chunkDetails", ImVec2(0, 200.0f), true);
            if (_selectedChunk >= 0 && _selectedChunk < static_cast<int>(_sifChunks.size()))
            {
                auto const& chunk = _sifChunks[static_cast<std::size_t>(_selectedChunk)];
                ImGui::Text("Name: %s", chunk.Name.c_str());
                ImGui::Text("Type: 0x%08X", chunk.TypeValue);
                ImGui::Text("Data: %u bytes", chunk.DataSize);
                ImGui::Text("Chunk: %u bytes", chunk.ChunkSize);
                ImGui::Text("Relocations: %zu", chunk.Relocations.size());
                ImGui::Text("Endian: %s", chunk.BigEndian ? "Big" : "Little");
                ImGui::Separator();
                if (chunk.TypeValue == MakeTypeCode('C', 'O', 'L', 'I'))
                {
                    if (ImGui::Button("Show Collision Mesh"))
                    {
                        _selectedChunk = static_cast<int>(&chunk - &_sifChunks[0]);
                        LoadCollisionDebugGeometry();
                    }
                    ImGui::SameLine();
                    ImGui::Checkbox("Draw", &_drawCollisionMesh);
                }
                else if (chunk.TypeValue == MakeTypeCode('F', 'O', 'R', 'E'))
                {
                    if (ImGui::Button("Show Forest Meshes"))
                    {
                        _selectedChunk = static_cast<int>(&chunk - &_sifChunks[0]);
                        LoadForestResources();
                    }
                    ImGui::SameLine();
                    if (ImGui::Checkbox("Draw", &_drawForestMeshes))
                        UpdateForestMeshRendering();
                    ImGui::SameLine();
                    if (ImGui::Checkbox("Draw boxes", &_drawForestBoxes))
                        UpdateForestBoxRenderer();
                    ImGui::SameLine();
                    if (ImGui::Button("Forest hierarchy"))
                        _showForestHierarchyWindow = true;
                    if (ImGui::Button("Export track.Forest OBJ"))
                    {
                        SeEditor::Dialogs::FileDialogOptions options;
                        options.Title = "Export track.Forest OBJ";
                        options.FilterPatterns = {"*.obj"};
                        options.FilterDescription = "Wavefront OBJ";
                        options.DefaultPathAndFile = _sifFilePath;
                        if (!options.DefaultPathAndFile.empty())
                        {
                            std::filesystem::path base = options.DefaultPathAndFile;
                            base.replace_extension(".obj");
                            options.DefaultPathAndFile = base.string();
                        }
                        if (auto result = SeEditor::Dialogs::TinyFileDialog::saveFileDialog(options))
                            ExportForestObj(*result, "track.Forest");
                    }
                }
                else
                {
                    ImGui::TextDisabled("No visualizer for this chunk yet.");
                }
            }
            ImGui::EndChild();
            ImGui::Separator();
        }

        bool showHex = !_sifParseError.empty() || _sifChunks.empty();
        if (showHex)
        {
            if (!_sifParseError.empty())
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), "Parse error: %s", _sifParseError.c_str());
            ImGui::TextWrapped("Showing raw hex dump of the data.");
            ImGui::BeginChild("SifContents", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
            for (auto const& line : _sifHexDump)
                ImGui::TextUnformatted(line.c_str());
            ImGui::EndChild();
        }
        else
        {
            ImGui::Text("Chunks: %zu", _sifChunks.size());
            ImGui::BeginChild("SifContents", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
            for (auto const& chunk : _sifChunks)
            {
                ImGui::Text("%s (0x%08X)", chunk.Name.c_str(), chunk.TypeValue);
                ImGui::Text("  Data: %u bytes, Chunk: %u bytes, Relocations: %zu", chunk.DataSize,
                            chunk.ChunkSize, chunk.Relocations.size());
                std::string relocLine = FormatRelocationList(chunk.Relocations);
                if (!relocLine.empty())
                    ImGui::TextUnformatted(relocLine.c_str());
                ImGui::Separator();
            }
            ImGui::EndChild();
        }

        if (_showForestHierarchyWindow)
        {
            ImGui::SetNextWindowSize(ImVec2(320.0f, 400.0f), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Forest Hierarchy", &_showForestHierarchyWindow))
            {
                if (_forestBoxLayers.empty())
                {
                    ImGui::Text("No forest data loaded yet.");
                }
                else
                {
                    bool hierarchyChanged = false;
                    for (auto& layer : _forestBoxLayers)
                        hierarchyChanged |= RenderForestBoxLayer(layer);
                    if (hierarchyChanged)
                    {
                        UpdateForestBoxRenderer();
                        UpdateForestMeshRendering();
                    }
                }
            }
            ImGui::End();
        }

    }

    ImGui::End();
}

void CharmyBee::OpenSifFile()
{
    using namespace SeEditor::Dialogs;
    FileDialogOptions options;
    options.Title = "Load SIF/ZIF file";
    options.DefaultPathAndFile = _sifFilePath;
    options.FilterPatterns = {"*.sif", "*.zif", "*.sig", "*.zig"};
    options.FilterDescription = "SIF/ZIF/SIG/ZIG files";

    if (auto result = TinyFileDialog::openFileDialog(options))
    {
        std::ifstream file(*result, std::ios::binary);
        if (!file)
        {
            _sifHexDump.clear();
            _sifFilePath.clear();
            _sifLoadMessage = "Failed to open file.";
            ReportSifError(_sifLoadMessage);
            return;
        }

        std::vector<char> buffer((std::istreambuf_iterator<char>(file)), {});
        std::vector<std::uint8_t> data(buffer.begin(), buffer.end());

        _sifHexDump = FormatHexDump(data);
        _sifFilePath = *result;
        _sifOriginalSize = data.size();
        _sifDecompressedSize = data.size();
        _sifFileCompressed = false;
        _sifChunks.clear();
        _sifGpuChunks.clear();
        _sifGpuRaw.clear();
        _sifParseError.clear();

        std::string parseError;
        std::optional<SifParseResult> parseResult = ParseSifFile(data, parseError);
        if (!parseResult)
        {
            _sifParseError = parseError;
            _sifLoadMessage = "Loaded " + std::to_string(data.size()) + " bytes, parse error: " + parseError;
            ReportSifError(_sifLoadMessage);
        }
        else
        {
            _sifChunks = std::move(parseResult->Chunks);
            _sifFileCompressed = parseResult->WasCompressed;
            _sifDecompressedSize = parseResult->DecompressedSize;
            _selectedChunk = !_sifChunks.empty() ? 0 : -1;

            _sifLoadMessage = "Loaded " + std::to_string(data.size()) + " bytes";
            if (_sifFileCompressed)
                _sifLoadMessage += " (decompressed to " + std::to_string(_sifDecompressedSize) + " bytes)";
            if (!_sifChunks.empty())
                _sifLoadMessage += ", parsed " + std::to_string(_sifChunks.size()) + " chunks.";

            _renderer.SetForestMeshes({});
            _renderer.SetDrawForestMeshes(false);
            _drawForestMeshes = false;
            _forestLibrary.reset();

            try
            {
                std::filesystem::path gpuPath = _sifFilePath;
                std::filesystem::path altGpuPath;
                if (gpuPath.extension() == ".sif")
                {
                    gpuPath.replace_extension(".zig");
                    altGpuPath = _sifFilePath;
                    altGpuPath.replace_extension(".sig");
                }
                else if (gpuPath.extension() == ".zif")
                {
                    gpuPath.replace_extension(".zig");
                    altGpuPath = _sifFilePath;
                    altGpuPath.replace_extension(".sig");
                }

                std::filesystem::path chosenGpuPath;
                if (std::filesystem::exists(gpuPath))
                    chosenGpuPath = gpuPath;
                else if (!altGpuPath.empty() && std::filesystem::exists(altGpuPath))
                    chosenGpuPath = altGpuPath;

                if (!chosenGpuPath.empty())
                {
                    std::ifstream gpuFile(chosenGpuPath, std::ios::binary);
                    if (gpuFile)
                    {
                        std::vector<char> gpuBuffer((std::istreambuf_iterator<char>(gpuFile)), {});
                        std::vector<std::uint8_t> gpuData(gpuBuffer.begin(), gpuBuffer.end());
                        bool compressed = false;
                        if (LooksLikeZlib(gpuData))
                        {
                            auto inflated = DecompressZlib(gpuData);
                            if (inflated.size() >= 4)
                                gpuData.assign(inflated.begin() + 4, inflated.end());
                            compressed = true;
                        }

                        if (!compressed)
                            StripLengthPrefixIfPresent(gpuData);

                        _sifGpuRaw.assign(gpuData.begin(), gpuData.end());
                        std::cout << "[CharmyBee] Using raw GPU buffer (" << _sifGpuRaw.size() << " bytes) for "
                                  << chosenGpuPath.string() << '.' << std::endl;
                    }
                }
            }
            catch (...) {}

            LoadCollisionDebugGeometry();
        }
    }
}

void CharmyBee::LoadCollisionDebugGeometry()
{
    _collisionVertices.clear();
    _collisionTriangles.clear();
    _renderer.SetCollisionMesh({}, {});
    _renderer.SetForestBoxes({});
    _forestBoxLayers.clear();
    _allForestMeshes.clear();
    UpdateForestMeshRendering();
    _renderer.SetDrawForestBoxes(false);
    _showForestHierarchyWindow = false;

    auto it = std::find_if(_sifChunks.begin(), _sifChunks.end(),
        [](SifChunkInfo const& c) { return c.TypeValue == MakeTypeCode('C', 'O', 'L', 'I'); });

    if (it == _sifChunks.end())
        return;

    std::string error;
    if (ParseCollisionMeshChunk(*it, _collisionVertices, _collisionTriangles, error))
    {
        _renderer.SetCollisionMesh(_collisionVertices, _collisionTriangles);
        if (!_collisionVertices.empty())
        {
            SlLib::Math::Vector3 min = _collisionVertices[0];
            SlLib::Math::Vector3 max = _collisionVertices[0];
            for (auto const& v : _collisionVertices)
            {
                min.X = std::min(min.X, v.X);
                min.Y = std::min(min.Y, v.Y);
                min.Z = std::min(min.Z, v.Z);
                max.X = std::max(max.X, v.X);
                max.Y = std::max(max.Y, v.Y);
                max.Z = std::max(max.Z, v.Z);
            }
            _collisionCenter = {(min.X + max.X) * 0.5f, (min.Y + max.Y) * 0.5f, (min.Z + max.Z) * 0.5f};
        }
        _orbitTarget = _collisionCenter;
        _orbitOffset = {0.0f, 0.0f, 0.0f};
        std::cout << "[CharmyBee] Collision mesh loaded: " << _collisionVertices.size()
                  << " vertices, " << _collisionTriangles.size() << " tris." << std::endl;
    }
    else
    {
        std::cerr << "[CharmyBee] Collision parse failed: " << error << std::endl;
    }
}

namespace {

struct BoxState
{
    bool Has = false;
    SlLib::Math::Vector3 Min{};
    SlLib::Math::Vector3 Max{};

    void Include(SlLib::Math::Vector3 const& point)
    {
        if (!Has)
        {
            Min = Max = point;
            Has = true;
            return;
        }

        Min.X = std::min(Min.X, point.X);
        Min.Y = std::min(Min.Y, point.Y);
        Min.Z = std::min(Min.Z, point.Z);
        Max.X = std::max(Max.X, point.X);
        Max.Y = std::max(Max.Y, point.Y);
        Max.Z = std::max(Max.Z, point.Z);
    }
};

} // namespace

void CharmyBee::LoadForestDebugGeometry()
{
    LoadForestResources();
}

void CharmyBee::LoadForestResources()
{
    _renderer.SetForestMeshes({});
    _renderer.SetDrawForestMeshes(false);
    _drawForestMeshes = false;
    _forestLibrary.reset();
    _allForestMeshes.clear();

    auto it = std::find_if(_sifChunks.begin(), _sifChunks.end(),
        [](SifChunkInfo const& c) { return c.TypeValue == MakeTypeCode('F', 'O', 'R', 'E'); });

    if (it == _sifChunks.end())
        return;

    auto const& chunk = *it;
    std::span<const std::uint8_t> cpuData(chunk.Data.data(), chunk.Data.size());
    std::span<const std::uint8_t> gpuData;
    if (!_sifGpuRaw.empty())
        gpuData = std::span<const std::uint8_t>(_sifGpuRaw.data(), _sifGpuRaw.size());
    else
    {
        auto gpuIt = std::find_if(_sifGpuChunks.begin(), _sifGpuChunks.end(),
            [](SifChunkInfo const& c) { return c.TypeValue == MakeTypeCode('F', 'O', 'R', 'E'); });
        if (gpuIt != _sifGpuChunks.end() && !gpuIt->Data.empty())
            gpuData = std::span<const std::uint8_t>(gpuIt->Data.data(), gpuIt->Data.size());
    }

    static SlLib::Resources::Database::SlPlatform s_win32("win32", false, false, 0);
    static SlLib::Resources::Database::SlPlatform s_xbox360("x360", true, false, 0);
    SlLib::Serialization::ResourceLoadContext context(cpuData, gpuData);
    bool isBigEndian = chunk.BigEndian;
    context.Platform = isBigEndian ? &s_xbox360 : &s_win32;

    auto library = std::make_shared<SeEditor::Forest::ForestLibrary>();
    try
    {
        library->Load(context);
    }
    catch (std::exception const& ex)
    {
        std::cerr << "[CharmyBee] Forest load failed: " << ex.what() << std::endl;
        return;
    }

    struct ForestVertex
    {
        SlLib::Math::Vector3 Pos{};
        SlLib::Math::Vector3 Normal{0.0f, 1.0f, 0.0f};
        SlLib::Math::Vector2 Uv{};
    };

    struct BoxState
    {
        bool Has = false;
        SlLib::Math::Vector3 Min{};
        SlLib::Math::Vector3 Max{};

        void Include(SlLib::Math::Vector3 const& point)
        {
            if (!Has)
            {
                Min = Max = point;
                Has = true;
                return;
            }
            Min.X = std::min(Min.X, point.X);
            Min.Y = std::min(Min.Y, point.Y);
            Min.Z = std::min(Min.Z, point.Z);
            Max.X = std::max(Max.X, point.X);
            Max.Y = std::max(Max.Y, point.Y);
            Max.Z = std::max(Max.Z, point.Z);
        }
    };

    auto readFloat = [](std::vector<std::uint8_t> const& data, std::size_t offset) -> float {
        if (offset + 4 > data.size())
            return 0.0f;
        float v = 0.0f;
        std::memcpy(&v, data.data() + offset, sizeof(float));
        return v;
    };
    auto readU16 = [](std::vector<std::uint8_t> const& data, std::size_t offset) -> std::uint16_t {
        if (offset + 2 > data.size())
            return 0;
        return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
    };
    auto readS16 = [&](std::vector<std::uint8_t> const& data, std::size_t offset) -> std::int16_t {
        return static_cast<std::int16_t>(readU16(data, offset));
    };

    auto decodeVertex = [&](SeEditor::Forest::SuRenderVertexStream const& stream) {
        std::vector<ForestVertex> verts;
        if (stream.VertexCount <= 0 || stream.VertexStride <= 0 || stream.Stream.empty())
            return verts;

        verts.resize(static_cast<std::size_t>(stream.VertexCount));
        for (int i = 0; i < stream.VertexCount; ++i)
        {
            std::size_t base = static_cast<std::size_t>(i) * static_cast<std::size_t>(stream.VertexStride);
            ForestVertex v;
            for (auto const& attr : stream.AttributeStreamsInfo)
            {
                if (attr.Stream != 0)
                    continue;

                std::size_t off = base + static_cast<std::size_t>(attr.Offset);
                using SeEditor::Forest::D3DDeclType;
                using SeEditor::Forest::D3DDeclUsage;

                if (attr.Usage == D3DDeclUsage::Position)
                {
                    std::size_t posOff = off;
                    if (stream.StreamBias != 0)
                        posOff += static_cast<std::size_t>(stream.StreamBias);
                    if (attr.Type == D3DDeclType::Float3)
                    {
                        v.Pos = {readFloat(stream.Stream, posOff + 0),
                                 readFloat(stream.Stream, posOff + 4),
                                 readFloat(stream.Stream, posOff + 8)};
                    }
                    else if (attr.Type == D3DDeclType::Float4)
                    {
                        v.Pos = {readFloat(stream.Stream, posOff + 0),
                                 readFloat(stream.Stream, posOff + 4),
                                 readFloat(stream.Stream, posOff + 8)};
                    }
                }
                else if (attr.Usage == D3DDeclUsage::Normal)
                {
                    if (attr.Type == D3DDeclType::Float3)
                    {
                        v.Normal = {readFloat(stream.Stream, off + 0),
                                    readFloat(stream.Stream, off + 4),
                                    readFloat(stream.Stream, off + 8)};
                    }
                    else if (attr.Type == D3DDeclType::Float16x4)
                    {
                        v.Normal = {HalfToFloat(readU16(stream.Stream, off + 0)),
                                    HalfToFloat(readU16(stream.Stream, off + 2)),
                                    HalfToFloat(readU16(stream.Stream, off + 4))};
                    }
                    else if (attr.Type == D3DDeclType::Short4N)
                    {
                        v.Normal = {readS16(stream.Stream, off + 0) / 32767.0f,
                                    readS16(stream.Stream, off + 2) / 32767.0f,
                                    readS16(stream.Stream, off + 4) / 32767.0f};
                    }
                }
                else if (attr.Usage == D3DDeclUsage::TexCoord)
                {
                    if (attr.Type == D3DDeclType::Float2)
                    {
                        v.Uv = {readFloat(stream.Stream, off + 0),
                                readFloat(stream.Stream, off + 4)};
                    }
                    else if (attr.Type == D3DDeclType::Float16x2)
                    {
                        v.Uv = {HalfToFloat(readU16(stream.Stream, off + 0)),
                                HalfToFloat(readU16(stream.Stream, off + 2))};
                    }
                }
            }

            verts[static_cast<std::size_t>(i)] = v;
        }

        return verts;
    };

    auto buildLocalMatrix = [](SlLib::Math::Vector4 t, SlLib::Math::Vector4 r, SlLib::Math::Vector4 s) {
        SlLib::Math::Quaternion q{r.X, r.Y, r.Z, r.W};
        SlLib::Math::Matrix4x4 rot = SlLib::Math::CreateFromQuaternion(q);
        SlLib::Math::Matrix4x4 scale{};
        scale(0, 0) = s.X;
        scale(1, 1) = s.Y;
        scale(2, 2) = s.Z;
        scale(3, 3) = 1.0f;
        SlLib::Math::Matrix4x4 local = SlLib::Math::Multiply(rot, scale);
        local(0, 3) = t.X;
        local(1, 3) = t.Y;
        local(2, 3) = t.Z;
        local(3, 3) = 1.0f;
        return local;
    };

    std::vector<Renderer::SlRenderer::ForestCpuMesh> meshes;
    std::vector<ForestBoxLayer> layers;
    layers.reserve(library->Forests.size());
    std::size_t debugDroppedLogged = 0;

    for (std::size_t forestIdx = 0; forestIdx < library->Forests.size(); ++forestIdx)
    {
        auto const& forestEntry = library->Forests[forestIdx];
        if (!forestEntry.Forest)
            continue;

        ForestBoxLayer layer;
        layer.Name = forestEntry.Name.empty()
                         ? std::string("Forest ") + std::to_string(forestIdx)
                         : forestEntry.Name;
        BoxState forestState;
        auto const& trees = forestEntry.Forest->Trees;
        struct TreeInfo
        {
            BoxState Bounds;
            std::size_t MeshStart = 0;
            std::size_t MeshCount = 0;
        };
        std::vector<TreeInfo> treeInfos(trees.size());

        for (std::size_t treeIdx = 0; treeIdx < trees.size(); ++treeIdx)
        {
            auto const& tree = trees[treeIdx];
            if (!tree)
                continue;

            TreeInfo& treeInfo = treeInfos[treeIdx];
            std::size_t branchCount = tree->Branches.size();
            std::vector<SlLib::Math::Matrix4x4> world(branchCount);
            std::vector<bool> computed(branchCount, false);
            std::size_t treeMeshStart = meshes.size();

            std::function<SlLib::Math::Matrix4x4(int)> computeWorld = [&](int idx) -> SlLib::Math::Matrix4x4 {
                if (idx < 0 || static_cast<std::size_t>(idx) >= branchCount)
                    return SlLib::Math::Matrix4x4{};
                if (computed[static_cast<std::size_t>(idx)])
                    return world[static_cast<std::size_t>(idx)];

                SlLib::Math::Vector4 t{};
                SlLib::Math::Vector4 r{};
                SlLib::Math::Vector4 s{1.0f, 1.0f, 1.0f, 1.0f};
                if (static_cast<std::size_t>(idx) < tree->Translations.size())
                    t = tree->Translations[static_cast<std::size_t>(idx)];
                if (static_cast<std::size_t>(idx) < tree->Rotations.size())
                    r = tree->Rotations[static_cast<std::size_t>(idx)];
                if (static_cast<std::size_t>(idx) < tree->Scales.size())
                    s = tree->Scales[static_cast<std::size_t>(idx)];

                auto local = buildLocalMatrix(t, r, s);
                int parentIndex = tree->Branches[static_cast<std::size_t>(idx)]->Parent;
                if (parentIndex >= 0 && parentIndex < static_cast<int>(branchCount))
                {
                    world[static_cast<std::size_t>(idx)] =
                        SlLib::Math::Multiply(computeWorld(parentIndex), local);
                }
                else
                {
                    world[static_cast<std::size_t>(idx)] = local;
                }

                computed[static_cast<std::size_t>(idx)] = true;
                return world[static_cast<std::size_t>(idx)];
            };

            auto appendMesh = [&](std::shared_ptr<SeEditor::Forest::SuRenderMesh> const& mesh,
                                  SlLib::Math::Matrix4x4 const& worldMatrix) {
                if (!mesh)
                    return;

                for (auto const& primitive : mesh->Primitives)
                {
                    if (!primitive || !primitive->VertexStream)
                        continue;

                    auto verts = decodeVertex(*primitive->VertexStream);
                    if (verts.empty())
                        continue;

                    SlLib::Math::Matrix4x4 normalMatrix = worldMatrix;
                    normalMatrix(0, 3) = 0.0f;
                    normalMatrix(1, 3) = 0.0f;
                    normalMatrix(2, 3) = 0.0f;

                    for (auto& v : verts)
                    {
                        SlLib::Math::Vector4 pos4{v.Pos.X, v.Pos.Y, v.Pos.Z, 1.0f};
                        auto transformed = SlLib::Math::Transform(worldMatrix, pos4);
                        v.Pos = {transformed.X, transformed.Y, transformed.Z};
                        treeInfo.Bounds.Include(v.Pos);
                        forestState.Include(v.Pos);

                        SlLib::Math::Vector4 n4{v.Normal.X, v.Normal.Y, v.Normal.Z, 0.0f};
                        auto nT = SlLib::Math::Transform(normalMatrix, n4);
                        v.Normal = SlLib::Math::normalize({nT.X, nT.Y, nT.Z});
                    }

                    Renderer::SlRenderer::ForestCpuMesh cpu;
                    cpu.Vertices.reserve(verts.size() * 8);
                    for (auto const& v : verts)
                    {
                        cpu.Vertices.push_back(v.Pos.X);
                        cpu.Vertices.push_back(v.Pos.Y);
                        cpu.Vertices.push_back(v.Pos.Z);
                        cpu.Vertices.push_back(v.Normal.X);
                        cpu.Vertices.push_back(v.Normal.Y);
                        cpu.Vertices.push_back(v.Normal.Z);
                        cpu.Vertices.push_back(v.Uv.X);
                        cpu.Vertices.push_back(v.Uv.Y);
                    }

                    std::size_t availableIndices = primitive->IndexData.size() / 2;
                    std::size_t indexCount = availableIndices;
                    if (primitive->NumIndices > 0)
                    {
                        if (static_cast<std::size_t>(primitive->NumIndices) > availableIndices)
                        {
                            std::cerr << "[Forest] Primitive index count " << primitive->NumIndices
                                      << " exceeds buffer (" << availableIndices << "), clamping.\n";
                        }
                        indexCount = std::min(static_cast<std::size_t>(primitive->NumIndices), availableIndices);
                    }

                    if (primitive->NumIndices <= 0 && availableIndices > 0)
                    {
                        std::cerr << "[Forest] Primitive reports zero indices but buffer contains "
                                  << availableIndices << " entries.\n";
                    }

                    if (indexCount == 0)
                        continue;

                    std::size_t vertexLimit = verts.size();
                    struct IndexMode
                    {
                        bool Use32 = false;
                        bool Swap = false;
                        std::size_t Count = 0;
                        std::size_t Droppable = 0;
                        std::size_t Restart = 0;
                        std::uint32_t MaxIndex = 0;
                    };

                    auto eval16 = [&](bool swap) {
                        IndexMode mode;
                        mode.Use32 = false;
                        mode.Swap = swap;
                        mode.Count = indexCount;
                        for (std::size_t i = 0; i < indexCount; ++i)
                        {
                            std::uint16_t a = primitive->IndexData[i * 2];
                            std::uint16_t b = primitive->IndexData[i * 2 + 1];
                            std::uint16_t idx = swap ? static_cast<std::uint16_t>((a << 8) | b)
                                                     : static_cast<std::uint16_t>(a | (b << 8));
                            if (idx == 0xFFFFu)
                            {
                                ++mode.Restart;
                                continue;
                            }
                            if (idx > mode.MaxIndex)
                                mode.MaxIndex = idx;
                            if (static_cast<std::size_t>(idx) >= vertexLimit)
                                ++mode.Droppable;
                        }
                        return mode;
                    };

                    auto eval32 = [&](bool swap) {
                        IndexMode mode;
                        mode.Use32 = true;
                        mode.Swap = swap;
                        if (primitive->IndexData.size() % 4 != 0)
                            return mode;
                        mode.Count = primitive->IndexData.size() / 4;
                        if (primitive->NumIndices > 0)
                            mode.Count = std::min<std::size_t>(mode.Count,
                                static_cast<std::size_t>(primitive->NumIndices));
                        for (std::size_t i = 0; i < mode.Count; ++i)
                        {
                            std::size_t off = i * 4;
                            std::uint32_t idx = static_cast<std::uint32_t>(primitive->IndexData[off + 0] |
                                (primitive->IndexData[off + 1] << 8) |
                                (primitive->IndexData[off + 2] << 16) |
                                (primitive->IndexData[off + 3] << 24));
                            if (swap)
                            {
                                idx = ((idx & 0x000000FFu) << 24) |
                                      ((idx & 0x0000FF00u) << 8) |
                                      ((idx & 0x00FF0000u) >> 8) |
                                      ((idx & 0xFF000000u) >> 24);
                            }
                            if (idx == 0xFFFFFFFFu)
                            {
                                ++mode.Restart;
                                continue;
                            }
                            if (idx > mode.MaxIndex)
                                mode.MaxIndex = idx;
                            if (idx >= vertexLimit)
                                ++mode.Droppable;
                        }
                        return mode;
                    };

                    IndexMode mode16le = eval16(false);
                    IndexMode mode16be = eval16(true);
                    IndexMode mode32le = eval32(false);
                    IndexMode mode32be = eval32(true);

                    IndexMode best = mode16le;
                    if (mode16be.Droppable < best.Droppable)
                        best = mode16be;
                    if (mode32le.Count > 0 && mode32le.Droppable < best.Droppable)
                        best = mode32le;
                    if (mode32be.Count > 0 && mode32be.Droppable < best.Droppable)
                        best = mode32be;

                    bool use32Bit = best.Use32;
                    bool swapIndices = best.Swap;
                    std::size_t indexCount32 = best.Use32 ? best.Count : 0;
                    std::size_t droppable = 0;
                    std::size_t restart = 0;
                    std::vector<std::uint32_t> rawIndices;
                    rawIndices.reserve(best.Count);

                    if (use32Bit)
                    {
                        for (std::size_t i = 0; i < indexCount32; ++i)
                        {
                            std::size_t off = i * 4;
                            std::uint32_t idx = static_cast<std::uint32_t>(primitive->IndexData[off + 0] |
                                (primitive->IndexData[off + 1] << 8) |
                                (primitive->IndexData[off + 2] << 16) |
                                (primitive->IndexData[off + 3] << 24));
                            if (swapIndices)
                            {
                                idx = ((idx & 0x000000FFu) << 24) |
                                      ((idx & 0x0000FF00u) << 8) |
                                      ((idx & 0x00FF0000u) >> 8) |
                                      ((idx & 0xFF000000u) >> 24);
                            }
                            if (idx == 0xFFFFFFFFu)
                            {
                                ++restart;
                                rawIndices.push_back(idx);
                                continue;
                            }
                            if (idx >= vertexLimit)
                            {
                                ++droppable;
                                continue;
                            }
                            rawIndices.push_back(idx);
                        }
                    }
                    else
                    {
                        for (std::size_t i = 0; i < best.Count; ++i)
                        {
                            std::uint16_t a = primitive->IndexData[i * 2];
                            std::uint16_t b = primitive->IndexData[i * 2 + 1];
                            std::uint16_t idx = swapIndices ? static_cast<std::uint16_t>((a << 8) | b)
                                                            : static_cast<std::uint16_t>(a | (b << 8));
                            if (idx == 0xFFFFu)
                            {
                                ++restart;
                                rawIndices.push_back(idx);
                                continue;
                            }
                            if (static_cast<std::size_t>(idx) >= vertexLimit)
                            {
                                ++droppable;
                                continue;
                            }
                            rawIndices.push_back(static_cast<std::uint32_t>(idx));
                        }
                    }

                    int primitiveType = primitive->Unknown_0x9c;
                    bool isStrip = primitiveType == 5 || (primitiveType != 4 && restart > 0);
                    if (isStrip)
                    {
                        cpu.Indices.reserve(rawIndices.size());
                        bool have0 = false;
                        bool have1 = false;
                        std::uint32_t i0 = 0;
                        std::uint32_t i1 = 0;
                        bool flip = false;
                        for (std::size_t i = 0; i < rawIndices.size(); ++i)
                        {
                            std::uint32_t idx = rawIndices[i];
                            if ((use32Bit && idx == 0xFFFFFFFFu) || (!use32Bit && idx == 0xFFFFu))
                            {
                                have0 = false;
                                have1 = false;
                                flip = false;
                                continue;
                            }
                            if (!have0)
                            {
                                i0 = idx;
                                have0 = true;
                                continue;
                            }
                            if (!have1)
                            {
                                i1 = idx;
                                have1 = true;
                                continue;
                            }

                            if (i0 != i1 && i1 != idx && i0 != idx)
                            {
                                if (flip)
                                {
                                    cpu.Indices.push_back(i1);
                                    cpu.Indices.push_back(i0);
                                    cpu.Indices.push_back(idx);
                                }
                                else
                                {
                                    cpu.Indices.push_back(i0);
                                    cpu.Indices.push_back(i1);
                                    cpu.Indices.push_back(idx);
                                }
                            }
                            i0 = i1;
                            i1 = idx;
                            flip = !flip;
                        }
                    }
                    else
                    {
                        cpu.Indices.reserve(rawIndices.size());
                        for (std::uint32_t idx : rawIndices)
                        {
                            if ((use32Bit && idx == 0xFFFFFFFFu) || (!use32Bit && idx == 0xFFFFu))
                                continue;
                            cpu.Indices.push_back(idx);
                        }
                    }

                    if (droppable > 0 || restart > 0)
                    {
                        if (droppable > 0)
                        {
                            std::cerr << "[Forest] Dropped " << droppable << " indices that referenced "
                                      << vertexLimit << " vertices.\n";
                        }
                        if (restart > 0)
                            std::cerr << "[Forest] Skipped " << restart << " primitive-restart indices.\n";
                        if (debugDroppedLogged < 5 && primitive->VertexStream)
                        {
                            ++debugDroppedLogged;
                            std::cerr << "[Forest] Debug: indices16 max=" << mode16le.MaxIndex
                                      << " dropped16=" << mode16le.Droppable
                                      << " restart16=" << mode16le.Restart
                                      << " indices32 max=" << mode32le.MaxIndex
                                      << " dropped32=" << mode32le.Droppable
                                      << " restart32=" << mode32le.Restart
                                      << " use32=" << (use32Bit ? "true" : "false")
                                      << " swap=" << (swapIndices ? "true" : "false")
                                      << " vtxCount=" << primitive->VertexStream->VertexCount
                                      << " stride=" << primitive->VertexStream->VertexStride
                                      << " streamBias=" << primitive->VertexStream->StreamBias
                                      << " endian=" << (isBigEndian ? "BE" : "LE")
                                      << " primType=" << primitiveType
                                      << '\n';
                        }
                    }

                    if (cpu.Indices.empty())
                        continue;

                    if (primitive->Material && !primitive->Material->Textures.empty())
                        cpu.Texture = primitive->Material->Textures[0]->TextureResource;
                    meshes.push_back(std::move(cpu));
                }
            };

            for (std::size_t i = 0; i < branchCount; ++i)
            {
                auto worldMatrix = computeWorld(static_cast<int>(i));
                auto const& branch = tree->Branches[i];
                if (!branch)
                    continue;

                if (branch->Mesh)
                    appendMesh(branch->Mesh, worldMatrix);
                if (branch->Lod)
                {
                    for (auto const& threshold : branch->Lod->Thresholds)
                    {
                        if (threshold && threshold->Mesh)
                            appendMesh(threshold->Mesh, worldMatrix);
                    }
                }
            }

            treeInfo.MeshStart = treeMeshStart;
            treeInfo.MeshCount = meshes.size() - treeMeshStart;
        }

        for (std::size_t treeIdx = 0; treeIdx < treeInfos.size(); ++treeIdx)
        {
            auto const& treeInfo = treeInfos[treeIdx];
            if (!treeInfo.Bounds.Has)
                continue;

            ForestBoxLayer child;
            child.Name = std::string("Tree ") + std::to_string(treeIdx);
            child.Visible = true;
            child.HasBounds = true;
            child.Min = treeInfo.Bounds.Min;
            child.Max = treeInfo.Bounds.Max;
            child.MeshStartIndex = treeInfo.MeshStart;
            child.MeshCount = treeInfo.MeshCount;
            layer.Children.push_back(std::move(child));
        }

        if (forestState.Has)
        {
            layer.HasBounds = true;
            layer.Min = forestState.Min;
            layer.Max = forestState.Max;
        }

        if (layer.HasBounds || !layer.Children.empty())
            layers.push_back(std::move(layer));
    }

    if (!meshes.empty())
    {
        _allForestMeshes = std::move(meshes);
        _drawForestMeshes = true;
        _forestLibrary = std::move(library);
        std::cout << "[CharmyBee] Forest meshes loaded: " << _forestLibrary->Forests.size() << " forests." << std::endl;
    }
    else
    {
        _allForestMeshes.clear();
    }

    _forestBoxLayers = std::move(layers);
    UpdateForestBoxRenderer();
    UpdateForestMeshRendering();
}

void CharmyBee::RebuildForestBoxHierarchy()
{
    UpdateForestBoxRenderer();
}

void CharmyBee::UpdateForestBoxRenderer()
{
    std::vector<std::pair<SlLib::Math::Vector3, SlLib::Math::Vector3>> boxes;
    boxes.reserve(_forestBoxLayers.size());

    std::function<void(ForestBoxLayer const&)> gather =
        [&](ForestBoxLayer const& layer) {
            if (layer.Visible && layer.HasBounds)
                boxes.emplace_back(layer.Min, layer.Max);
            for (auto const& child : layer.Children)
                gather(child);
        };

    for (auto const& layer : _forestBoxLayers)
        gather(layer);

    _renderer.SetForestBoxes(std::move(boxes));
    _renderer.SetDrawForestBoxes(_drawForestBoxes);
}

void CharmyBee::UpdateForestMeshRendering()
{
    if (_allForestMeshes.empty())
    {
        _renderer.SetForestMeshes({});
        _renderer.SetDrawForestMeshes(false);
        return;
    }

    std::vector<Renderer::SlRenderer::ForestCpuMesh> filtered;
    filtered.reserve(_allForestMeshes.size());

    auto gatherMeshes = [&](auto&& self, ForestBoxLayer const& layer) -> void {
        if (!layer.Visible)
            return;

        if (layer.MeshCount > 0 && layer.MeshStartIndex < _allForestMeshes.size())
        {
            std::size_t end = std::min(layer.MeshStartIndex + layer.MeshCount, _allForestMeshes.size());
            filtered.insert(filtered.end(),
                            _allForestMeshes.begin() + layer.MeshStartIndex,
                            _allForestMeshes.begin() + end);
        }

        for (auto const& child : layer.Children)
            self(self, child);
    };

    for (auto const& layer : _forestBoxLayers)
        gatherMeshes(gatherMeshes, layer);

    bool hasVisible = !filtered.empty();
    _renderer.SetForestMeshes(std::move(filtered));
    _renderer.SetDrawForestMeshes(_drawForestMeshes && hasVisible);
}

bool CharmyBee::RenderForestBoxLayer(ForestBoxLayer& layer)
{
    std::string label = layer.Name.empty() ? "Unnamed" : layer.Name;
    label += "###forest_" + std::to_string(reinterpret_cast<std::uintptr_t>(&layer));
    bool changed = ImGui::Checkbox(label.c_str(), &layer.Visible);
    if (changed)
    {
        for (auto& child : layer.Children)
            SetForestLayerVisibilityRecursive(child, layer.Visible);
    }
    if (!layer.Children.empty())
    {
        ImGui::Indent();
        for (auto& child : layer.Children)
            changed |= RenderForestBoxLayer(child);
        ImGui::Unindent();
    }
    return changed;
}

void CharmyBee::SetForestLayerVisibilityRecursive(ForestBoxLayer& layer, bool visible)
{
    layer.Visible = visible;
    for (auto& child : layer.Children)
        SetForestLayerVisibilityRecursive(child, visible);
}

void CharmyBee::ExportForestObj(std::filesystem::path const& outputPath,
                                std::string const& forestNameFilter)
{
    if (!_forestLibrary)
        LoadForestResources();

    if (!_forestLibrary || _forestLibrary->Forests.empty())
    {
        std::cerr << "[CharmyBee] No forest data loaded to export." << std::endl;
        return;
    }

    std::filesystem::path outDir = outputPath.parent_path();
    if (outDir.empty())
        outDir = std::filesystem::current_path();

    std::filesystem::path mtlPath = outputPath;
    mtlPath.replace_extension(".mtl");

    std::ofstream obj(outputPath, std::ios::binary);
    if (!obj)
    {
        std::cerr << "[CharmyBee] Failed to open OBJ file for writing: " << outputPath.string() << std::endl;
        return;
    }

    std::ofstream mtl(mtlPath, std::ios::binary);
    if (!mtl)
    {
        std::cerr << "[CharmyBee] Failed to open MTL file for writing: " << mtlPath.string() << std::endl;
        return;
    }

    auto sanitize = [](std::string name) {
        for (char& c : name)
        {
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.'))
                c = '_';
        }
        if (name.empty())
            name = "unnamed";
        return name;
    };

    struct ForestVertex
    {
        SlLib::Math::Vector3 Pos{};
        SlLib::Math::Vector3 Normal{0.0f, 1.0f, 0.0f};
        SlLib::Math::Vector2 Uv{};
    };

    auto readFloat = [](std::vector<std::uint8_t> const& data, std::size_t offset) -> float {
        if (offset + 4 > data.size())
            return 0.0f;
        float v = 0.0f;
        std::memcpy(&v, data.data() + offset, sizeof(float));
        return v;
    };
    auto readU16 = [](std::vector<std::uint8_t> const& data, std::size_t offset) -> std::uint16_t {
        if (offset + 2 > data.size())
            return 0;
        return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
    };
    auto readS16 = [&](std::vector<std::uint8_t> const& data, std::size_t offset) -> std::int16_t {
        return static_cast<std::int16_t>(readU16(data, offset));
    };

    auto decodeVertex = [&](SeEditor::Forest::SuRenderVertexStream const& stream) {
        std::vector<ForestVertex> verts;
        if (stream.VertexCount <= 0 || stream.VertexStride <= 0 || stream.Stream.empty())
            return verts;

        verts.resize(static_cast<std::size_t>(stream.VertexCount));
        for (int i = 0; i < stream.VertexCount; ++i)
        {
            std::size_t base = static_cast<std::size_t>(i) * static_cast<std::size_t>(stream.VertexStride);
            ForestVertex v;
            for (auto const& attr : stream.AttributeStreamsInfo)
            {
                if (attr.Stream != 0)
                    continue;

                std::size_t off = base + static_cast<std::size_t>(attr.Offset);
                using SeEditor::Forest::D3DDeclType;
                using SeEditor::Forest::D3DDeclUsage;

                if (attr.Usage == D3DDeclUsage::Position)
                {
                    std::size_t posOff = off;
                    if (stream.StreamBias != 0)
                        posOff += static_cast<std::size_t>(stream.StreamBias);
                    if (attr.Type == D3DDeclType::Float3)
                    {
                        v.Pos = {readFloat(stream.Stream, posOff + 0),
                                 readFloat(stream.Stream, posOff + 4),
                                 readFloat(stream.Stream, posOff + 8)};
                    }
                    else if (attr.Type == D3DDeclType::Float4)
                    {
                        v.Pos = {readFloat(stream.Stream, posOff + 0),
                                 readFloat(stream.Stream, posOff + 4),
                                 readFloat(stream.Stream, posOff + 8)};
                    }
                }
                else if (attr.Usage == D3DDeclUsage::Normal)
                {
                    if (attr.Type == D3DDeclType::Float3)
                    {
                        v.Normal = {readFloat(stream.Stream, off + 0),
                                    readFloat(stream.Stream, off + 4),
                                    readFloat(stream.Stream, off + 8)};
                    }
                    else if (attr.Type == D3DDeclType::Float16x4)
                    {
                        v.Normal = {HalfToFloat(readU16(stream.Stream, off + 0)),
                                    HalfToFloat(readU16(stream.Stream, off + 2)),
                                    HalfToFloat(readU16(stream.Stream, off + 4))};
                    }
                    else if (attr.Type == D3DDeclType::Short4N)
                    {
                        v.Normal = {readS16(stream.Stream, off + 0) / 32767.0f,
                                    readS16(stream.Stream, off + 2) / 32767.0f,
                                    readS16(stream.Stream, off + 4) / 32767.0f};
                    }
                }
                else if (attr.Usage == D3DDeclUsage::TexCoord)
                {
                    if (attr.Type == D3DDeclType::Float2)
                    {
                        v.Uv = {readFloat(stream.Stream, off + 0),
                                readFloat(stream.Stream, off + 4)};
                    }
                    else if (attr.Type == D3DDeclType::Float16x2)
                    {
                        v.Uv = {HalfToFloat(readU16(stream.Stream, off + 0)),
                                HalfToFloat(readU16(stream.Stream, off + 2))};
                    }
                }
            }

            verts[static_cast<std::size_t>(i)] = v;
        }

        return verts;
    };

    auto buildLocalMatrix = [](SlLib::Math::Vector4 t, SlLib::Math::Vector4 r, SlLib::Math::Vector4 s) {
        SlLib::Math::Quaternion q{r.X, r.Y, r.Z, r.W};
        SlLib::Math::Matrix4x4 rot = SlLib::Math::CreateFromQuaternion(q);
        SlLib::Math::Matrix4x4 scale{};
        scale(0, 0) = s.X;
        scale(1, 1) = s.Y;
        scale(2, 2) = s.Z;
        scale(3, 3) = 1.0f;
        SlLib::Math::Matrix4x4 local = SlLib::Math::Multiply(rot, scale);
        local(0, 3) = t.X;
        local(1, 3) = t.Y;
        local(2, 3) = t.Z;
        local(3, 3) = 1.0f;
        return local;
    };

    std::unordered_map<SeEditor::Forest::SuRenderTextureResource*, std::string> materialNames;
    std::unordered_map<SeEditor::Forest::SuRenderTextureResource*, std::string> textureFiles;
    int materialCounter = 0;

    auto getMaterialName = [&](std::shared_ptr<SeEditor::Forest::SuRenderTextureResource> const& tex) {
        if (!tex)
            return std::string("default");
        auto* key = tex.get();
        auto it = materialNames.find(key);
        if (it != materialNames.end())
            return it->second;

        std::string base = tex->Name.empty() ? ("tex_" + std::to_string(materialCounter++))
                                             : sanitize(std::filesystem::path(tex->Name).filename().string());
        std::string mtlName = base;
        materialNames[key] = mtlName;

        if (!tex->ImageData.empty())
        {
            std::string texFile = base;
            if (std::filesystem::path(texFile).extension().empty())
                texFile += ".dds";
            textureFiles[key] = texFile;
            std::filesystem::path outTex = outDir / texFile;
            std::ofstream texOut(outTex, std::ios::binary);
            if (texOut)
                texOut.write(reinterpret_cast<char const*>(tex->ImageData.data()),
                             static_cast<std::streamsize>(tex->ImageData.size()));
        }

        return mtlName;
    };

    obj << "# Exported from CppSLib Forest\n";
    obj << "mtllib " << mtlPath.filename().string() << "\n";

    std::size_t globalVertexBase = 1;
    int objectCounter = 0;

    for (auto const& forestEntry : _forestLibrary->Forests)
    {
        if (!forestEntry.Forest)
            continue;
        if (!forestNameFilter.empty() && forestEntry.Name != forestNameFilter)
            continue;

        for (auto const& tree : forestEntry.Forest->Trees)
        {
            if (!tree)
                continue;

            std::size_t branchCount = tree->Branches.size();
            std::vector<SlLib::Math::Matrix4x4> world(branchCount);
            std::vector<bool> computed(branchCount, false);

            std::function<SlLib::Math::Matrix4x4(int)> computeWorld = [&](int idx) -> SlLib::Math::Matrix4x4 {
                if (idx < 0 || static_cast<std::size_t>(idx) >= branchCount)
                    return SlLib::Math::Matrix4x4{};
                if (computed[static_cast<std::size_t>(idx)])
                    return world[static_cast<std::size_t>(idx)];

                SlLib::Math::Vector4 t{};
                SlLib::Math::Vector4 r{};
                SlLib::Math::Vector4 s{1.0f, 1.0f, 1.0f, 1.0f};
                if (static_cast<std::size_t>(idx) < tree->Translations.size())
                    t = tree->Translations[static_cast<std::size_t>(idx)];
                if (static_cast<std::size_t>(idx) < tree->Rotations.size())
                    r = tree->Rotations[static_cast<std::size_t>(idx)];
                if (static_cast<std::size_t>(idx) < tree->Scales.size())
                    s = tree->Scales[static_cast<std::size_t>(idx)];

                auto local = buildLocalMatrix(t, r, s);
                int parentIndex = tree->Branches[static_cast<std::size_t>(idx)]->Parent;
                if (parentIndex >= 0 && parentIndex < static_cast<int>(branchCount))
                {
                    world[static_cast<std::size_t>(idx)] =
                        SlLib::Math::Multiply(computeWorld(parentIndex), local);
                }
                else
                {
                    world[static_cast<std::size_t>(idx)] = local;
                }

                computed[static_cast<std::size_t>(idx)] = true;
                return world[static_cast<std::size_t>(idx)];
            };

            auto appendMesh = [&](std::shared_ptr<SeEditor::Forest::SuRenderMesh> const& mesh,
                                  SlLib::Math::Matrix4x4 const& worldMatrix) {
                if (!mesh)
                    return;

                for (auto const& primitive : mesh->Primitives)
                {
                    if (!primitive || !primitive->VertexStream)
                        continue;

                    auto verts = decodeVertex(*primitive->VertexStream);
                    if (verts.empty())
                        continue;

                    SlLib::Math::Matrix4x4 normalMatrix = worldMatrix;
                    normalMatrix(0, 3) = 0.0f;
                    normalMatrix(1, 3) = 0.0f;
                    normalMatrix(2, 3) = 0.0f;

                    for (auto& v : verts)
                    {
                        SlLib::Math::Vector4 pos4{v.Pos.X, v.Pos.Y, v.Pos.Z, 1.0f};
                        auto transformed = SlLib::Math::Transform(worldMatrix, pos4);
                        v.Pos = {transformed.X, transformed.Y, transformed.Z};

                        SlLib::Math::Vector4 n4{v.Normal.X, v.Normal.Y, v.Normal.Z, 0.0f};
                        auto nT = SlLib::Math::Transform(normalMatrix, n4);
                        v.Normal = SlLib::Math::normalize({nT.X, nT.Y, nT.Z});
                    }

                    std::string objName = sanitize(forestEntry.Name) + "_" + std::to_string(objectCounter++);
                    obj << "o " << objName << "\n";

                    std::string material = "default";
                    if (primitive->Material && !primitive->Material->Textures.empty())
                        material = getMaterialName(primitive->Material->Textures[0]->TextureResource);
                    obj << "usemtl " << material << "\n";

                    for (auto const& v : verts)
                        obj << "v " << v.Pos.X << " " << v.Pos.Y << " " << v.Pos.Z << "\n";
                    for (auto const& v : verts)
                        obj << "vt " << v.Uv.X << " " << (1.0f - v.Uv.Y) << "\n";
                    for (auto const& v : verts)
                        obj << "vn " << v.Normal.X << " " << v.Normal.Y << " " << v.Normal.Z << "\n";

                    std::size_t indexCount = primitive->IndexData.size() / 2;
                    auto getIndex = [&](std::size_t i) -> std::uint16_t {
                        return static_cast<std::uint16_t>(
                            primitive->IndexData[i * 2] |
                            (primitive->IndexData[i * 2 + 1] << 8));
                    };

                    auto emitTri = [&](std::uint16_t i0, std::uint16_t i1, std::uint16_t i2) {
                        std::size_t base = globalVertexBase;
                        obj << "f "
                            << (base + i0) << "/" << (base + i0) << "/" << (base + i0) << " "
                            << (base + i1) << "/" << (base + i1) << "/" << (base + i1) << " "
                            << (base + i2) << "/" << (base + i2) << "/" << (base + i2) << "\n";
                    };

                    std::size_t vertCount = verts.size();
                    if (vertCount == 0 || indexCount < 3)
                    {
                        globalVertexBase += verts.size();
                        continue;
                    }

                    auto countListInvalid = [&]() -> std::size_t {
                        std::size_t invalid = 0;
                        for (std::size_t i = 0; i + 2 < indexCount; i += 3)
                        {
                            std::uint16_t i0 = getIndex(i + 0);
                            std::uint16_t i1 = getIndex(i + 1);
                            std::uint16_t i2 = getIndex(i + 2);
                            if (i0 >= vertCount || i1 >= vertCount || i2 >= vertCount)
                                ++invalid;
                        }
                        return invalid;
                    };

                    auto countStripValid = [&]() -> std::size_t {
                        std::size_t valid = 0;
                        bool flip = false;
                        for (std::size_t i = 0; i + 2 < indexCount; ++i)
                        {
                            std::uint16_t i0 = getIndex(i + 0);
                            std::uint16_t i1 = getIndex(i + 1);
                            std::uint16_t i2 = getIndex(i + 2);
                            if (i0 == 0xFFFF || i1 == 0xFFFF || i2 == 0xFFFF)
                            {
                                flip = false;
                                continue;
                            }
                            if (i0 >= vertCount || i1 >= vertCount || i2 >= vertCount)
                                continue;
                            if (i0 == i1 || i1 == i2 || i0 == i2)
                                continue;
                            ++valid;
                            flip = !flip;
                        }
                        return valid;
                    };

                    bool useStrip = (indexCount % 3) != 0;
                    std::size_t invalidList = countListInvalid();
                    if (!useStrip && invalidList > (indexCount / 3) / 20)
                    {
                        std::size_t stripValid = countStripValid();
                        if (stripValid > (indexCount / 3) / 2)
                            useStrip = true;
                    }

                    if (!useStrip)
                    {
                        for (std::size_t i = 0; i + 2 < indexCount; i += 3)
                        {
                            std::uint16_t i0 = getIndex(i + 0);
                            std::uint16_t i1 = getIndex(i + 1);
                            std::uint16_t i2 = getIndex(i + 2);
                            if (i0 >= vertCount || i1 >= vertCount || i2 >= vertCount)
                                continue;
                            emitTri(i0, i1, i2);
                        }
                    }
                    else
                    {
                        bool flip = false;
                        for (std::size_t i = 0; i + 2 < indexCount; ++i)
                        {
                            std::uint16_t i0 = getIndex(i + 0);
                            std::uint16_t i1 = getIndex(i + 1);
                            std::uint16_t i2 = getIndex(i + 2);
                            if (i0 == 0xFFFF || i1 == 0xFFFF || i2 == 0xFFFF)
                            {
                                flip = false;
                                continue;
                            }
                            if (i0 >= vertCount || i1 >= vertCount || i2 >= vertCount)
                                continue;
                            if (i0 == i1 || i1 == i2 || i0 == i2)
                                continue;
                            if (!flip)
                                emitTri(i0, i1, i2);
                            else
                                emitTri(i1, i0, i2);
                            flip = !flip;
                        }
                    }

                    globalVertexBase += verts.size();
                }
            };

            for (std::size_t i = 0; i < branchCount; ++i)
            {
                auto const& branch = tree->Branches[i];
                if (!branch)
                    continue;
                auto worldMatrix = computeWorld(static_cast<int>(i));

                if (branch->Mesh)
                    appendMesh(branch->Mesh, worldMatrix);
                if (branch->Lod)
                {
                    for (auto const& threshold : branch->Lod->Thresholds)
                    {
                        if (threshold && threshold->Mesh)
                            appendMesh(threshold->Mesh, worldMatrix);
                    }
                }
            }
        }
    }

    if (materialNames.empty())
        materialNames[nullptr] = "default";

    for (auto const& entry : materialNames)
    {
        std::string const& name = entry.second;
        mtl << "newmtl " << name << "\n";
        mtl << "Ka 0 0 0\n";
        mtl << "Kd 1 1 1\n";
        mtl << "Ks 0 0 0\n";
        auto texIt = textureFiles.find(entry.first);
        if (texIt != textureFiles.end())
            mtl << "map_Kd " << texIt->second << "\n";
        mtl << "\n";
    }

    std::cout << "[CharmyBee] Exported forest OBJ to " << outputPath.string() << std::endl;
}

void CharmyBee::PollGlfwKeyInput()
{
    if (!_debugKeyInput || !_controller)
        return;

    auto* controller = _controller.get();
    GLFWwindow* window = controller->Window();
    if (window == nullptr)
        return;

    for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key)
    {
        int state = glfwGetKey(window, key);
        bool down = state == GLFW_PRESS || state == GLFW_REPEAT;
        if (down != _glfwKeyStates[key])
        {
            _glfwKeyStates[key] = down;
            std::string name = DescribeGlfwKey(key);
            std::cout << "[CharmyBee][KeyInput] " << (down ? "Pressed " : "Released ")
                      << name << " (" << key << ")" << std::endl;
        }
    }

    for (int button = GLFW_MOUSE_BUTTON_LEFT; button <= GLFW_MOUSE_BUTTON_LAST; ++button)
    {
        int state = glfwGetMouseButton(window, button);
        bool down = state == GLFW_PRESS || state == GLFW_REPEAT;
        if (down != _glfwMouseButtonStates[button])
        {
            _glfwMouseButtonStates[button] = down;
            std::string desc;
            switch (button)
            {
            case GLFW_MOUSE_BUTTON_LEFT: desc = "Left"; break;
            case GLFW_MOUSE_BUTTON_RIGHT: desc = "Right"; break;
            case GLFW_MOUSE_BUTTON_MIDDLE: desc = "Middle"; break;
            default: desc = "Mouse-" + std::to_string(button); break;
            }
            std::cout << "[CharmyBee][MouseInput] " << (down ? "Pressed " : "Released ")
                      << desc << " (" << button << ")" << std::endl;
        }
    }
}


void CharmyBee::ConvertSifWithWorkspace()
{
    if (_sifFilePath.empty())
    {
        _sifLoadMessage = "Load a SIF file before converting.";
        ReportSifError(_sifLoadMessage);
        return;
    }

    namespace fs = std::filesystem;
    fs::path project = fs::absolute(fs::path("..") / "SlMod" / "SlLib.Workspace" / "SlLib.Workspace.csproj");
    if (!fs::exists(project))
    {
        fs::path repoRoot = fs::path(__FILE__).parent_path().parent_path().parent_path();
        fs::path alternate = repoRoot / "SlMod" / "SlLib.Workspace" / "SlLib.Workspace.csproj";
        if (fs::exists(alternate))
        {
            project = alternate;
        }
        else
        {
            _sifLoadMessage = "SlMod workspace project not found: " + project.string();
            ReportSifError(_sifLoadMessage);
            return;
        }
    }

    fs::path original = fs::path(_sifFilePath);
    fs::path base = original;
    base.replace_extension();

    std::vector<fs::path> searchDirs;
    fs::path originalParent = original.parent_path();
    if (!originalParent.empty())
        searchDirs.push_back(originalParent);
    fs::path grandParent = originalParent.parent_path();
    fs::path datasetRoot;
    if (!grandParent.empty())
    {
        searchDirs.push_back(grandParent);
        searchDirs.push_back(grandParent / "Tracks");
        searchDirs.push_back(grandParent / "Original");
        datasetRoot = grandParent.parent_path();

        if (!datasetRoot.empty())
        {
            searchDirs.push_back(datasetRoot / "Research_Tracks");
            searchDirs.push_back(datasetRoot / "ResearchTrackStructure" / "Tracks");
            searchDirs.push_back(datasetRoot / "ResearchTrackStructure" / "Original");
        }
    }
    fs::path repoRoot = fs::path(__FILE__).parent_path().parent_path().parent_path();
    searchDirs.push_back(repoRoot / "ResearchTrackStructure");
    searchDirs.push_back(repoRoot / "ResearchTrackStructure" / "Tracks");
    searchDirs.push_back(repoRoot / "ResearchTrackStructure" / "Original");
    searchDirs.push_back(repoRoot / "Research_Tracks");

    std::string baseName = original.stem().string();
    fs::path targetZif = originalParent / (baseName + ".zif");
    fs::path targetZig = originalParent / (baseName + ".zig");

    const std::vector<std::string> cpuExtensions = {".zif", ".sif", ".sig"};
    const std::vector<std::string> gpuExtensions = {".zig"};
    std::optional<fs::path> cpuInput;
    std::optional<fs::path> gpuInput;

    auto matchesAny = [&](std::string const& candidate, std::vector<std::string> const& list) -> bool {
        for (auto const& ext : list)
        {
            if (_stricmp(candidate.c_str(), ext.c_str()) == 0)
                return true;
        }
        return false;
    };

    auto considerCandidate = [&](fs::path const& candidate) {
        std::string stem = candidate.stem().string();
        if (_stricmp(stem.c_str(), baseName.c_str()) != 0)
            return;

        std::string entryExt = candidate.extension().string();
        if (!gpuInput && matchesAny(entryExt, gpuExtensions))
        {
            gpuInput = candidate;
            return;
        }

        if (!cpuInput && matchesAny(entryExt, cpuExtensions))
        {
            cpuInput = candidate;
        }
    };

    considerCandidate(original);

    auto searchInDirectory = [&](fs::path const& dir) {
        if (dir.empty() || !fs::exists(dir))
            return;

        for (auto const& entry : fs::recursive_directory_iterator(dir))
        {
            if (!entry.is_regular_file())
                continue;

            considerCandidate(entry.path());
            if (cpuInput && gpuInput)
                return;
        }
    };

    for (auto const& dir : searchDirs)
    {
        if (cpuInput && gpuInput)
            break;
        searchInDirectory(dir);
    }

    if (!(cpuInput && gpuInput))
    {
        fs::path current = originalParent;
        while (!current.empty() && !(cpuInput && gpuInput))
        {
            searchInDirectory(current);
            current = current.parent_path();
        }
    }

    if (!(cpuInput && gpuInput) && datasetRoot != fs::path())
    {
        searchInDirectory(datasetRoot);
    }

    if (!(cpuInput && gpuInput))
    {
        fs::path root = repoRoot;
        searchInDirectory(root);
    }

    if (!cpuInput)
    {
        _sifLoadMessage = "Unable to locate CPU SIF/ZIF/SIG input for converter: " + base.string();
        ReportSifError(_sifLoadMessage);
        return;
    }

    if (!gpuInput)
    {
        _sifLoadMessage =
            "Converter requires a GPU ZIG file (" + (targetZig.filename().string()) +
            ") next to the SIF input before conversion can run.";
        ReportSifError(_sifLoadMessage);
        return;
    }

    auto copyInput = [&](fs::path const& source, fs::path const& target) -> bool {
        if (source == target)
            return true;

        try
        {
            fs::create_directories(target.parent_path());
            if (fs::exists(target))
                fs::remove(target);
            fs::copy_file(source, target, fs::copy_options::overwrite_existing);
            return true;
        }
        catch (std::exception const& ex)
        {
            _sifLoadMessage = "Failed to copy " + source.filename().string() +
                " to workspace directory: " + std::string(ex.what());
            ReportSifError(_sifLoadMessage);
            return false;
        }
    };

    if (!copyInput(*cpuInput, targetZif))
        return;
    if (!copyInput(*gpuInput, targetZig))
        return;

    if (!fs::exists(targetZif))
    {
        _sifLoadMessage = "Unable to prepare ZIF input for converter: " + targetZif.string();
        ReportSifError(_sifLoadMessage);
        return;
    }

    if (!fs::exists(targetZig))
    {
        _sifLoadMessage = "Unable to prepare ZIG input for converter: " + targetZig.string();
        ReportSifError(_sifLoadMessage);
        return;
    }

    std::string command = std::string("dotnet run --project ") + QuoteArgument(project.string()) +
        " convert-siff " + QuoteArgument(base.string()) + " " + QuoteArgument(base.string());

    int exitCode = std::system(command.c_str());
    if (exitCode == 0)
        _sifLoadMessage = "SlMod converter wrote " + base.string() + ".dat/.gpu.";
    else
    {
        _sifLoadMessage = "Converter exit code " + std::to_string(exitCode) + ".";
        ReportSifError(_sifLoadMessage);
    }
}

void CharmyBee::RenderMainDockWindow()
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(_width), static_cast<float>(_height)));
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBackground;

#ifdef ImGuiWindowFlags_NoDocking
    flags |= ImGuiWindowFlags_NoDocking;
#endif

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    bool open = true;
    bool show = ImGui::Begin("Main", &open, flags);
    ImGui::PopStyleVar();

    if (show)
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Close Workspace"))
                    _requestedWorkspaceClose = true;

                if (ImGui::MenuItem("DEBUG SAVE TO DESKTOP"))
                {
                    std::cout << "[CharmyBee] Debug save to desktop invoked.\n";
                }

                if (ImGui::MenuItem("DEBUG SAVE TO SEASIDEHILL"))
                {
                    std::cout << "[CharmyBee] Debug save to Seaside Hill invoked.\n";
                }

                if (ImGui::MenuItem("DEBUG SAVE ALL"))
                {
                    std::cout << "[CharmyBee] Debug save all invoked.\n";
                }

                if (ImGui::MenuItem("Load SIF File..."))
                {
                    OpenSifFile();
                }
                if (ImGui::MenuItem("Convert SIF via SlMod Workspace"))
                {
                    ConvertSifWithWorkspace();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Hierarchy", nullptr, &_showHierarchyWindow);
                if (ImGui::MenuItem("Forest hierarchy", nullptr, &_showForestHierarchyWindow))
                    UpdateForestBoxRenderer();
                ImGui::EndMenu();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3.0f, 4.0f));
            if (ImGui::BeginMenu("Nodes"))
            {
                DrawNodeCreationMenu();
                ImGui::EndMenu();
            }
            ImGui::PopStyleVar();

            ImVec2 cursorPos = ImGui::GetCursorPos();
            cursorPos.x += 10.0f;
            ImGui::SetCursorPos(cursorPos);
            if (ImGui::BeginTabBar("##modes"))
            {
                if (ImGui::BeginTabItem("Hierarchy"))
                {
                    ImGui::TextWrapped("Hierarchy and definition panels stay in their own window to the side.");
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Layout"))
                    ImGui::EndTabItem();

                if (ImGui::BeginTabItem("Navigation"))
                    ImGui::EndTabItem();

                ImGui::EndTabBar();
            }

            float width = ImGui::GetWindowWidth();
            float framerate = ImGui::GetIO().Framerate;
            ImGui::SetCursorPosX(width - 100);
            ImGui::Text("(%.1f FPS)", framerate);
            ImGui::EndMainMenuBar();
        }
    }

    ImGui::End();
}

void CharmyBee::RenderPanelWindow(char const* title, Editor::Panel::IEditorPanel* panel)
{
    ImGui::Begin(title);
    if (panel)
        panel->OnImGuiRender();
    ImGui::End();
}

void CharmyBee::Run()
{
    std::cout << "[CharmyBee] Initializing " << _title << " @ " << _width << "x" << _height << std::endl;

    _controller = std::make_unique<Graphics::ImGui::ImGuiController>(_width, _height);
    _assetPanel = std::make_unique<Editor::Panel::AssetPanel>();
    _scenePanel = std::make_unique<Editor::Panel::ScenePanel>();
    _inspectorPanel = std::make_unique<Editor::Panel::InspectorPanel>();

    OnLoad();
    PrintSceneInfo();

    auto previousTime = std::chrono::steady_clock::now();
    while (!_controller->ShouldClose())
    {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> delta = now - previousTime;
        previousTime = now;

        _controller->SetPerFrameImGuiData(delta.count());
        _controller->NewFrame();

        RenderMainDockWindow();
        if (_database)
            RenderWorkspace();
        else
            RenderProjectSelector();

        RenderHierarchyWindow();

        RenderSifViewer();

        if (_requestedWorkspaceClose)
            TriggerCloseWorkspace();

        _renderer.Render();
        _controller->Render();
        _controller->SwapBuffers();
        _controller->PollEvents();
        PollGlfwKeyInput();

        UpdateOrbitFromInput(delta.count());
    }

    _controller->Dispose();
    std::cout << "[CharmyBee] Shutdown complete." << std::endl;
}

} // namespace SeEditor
