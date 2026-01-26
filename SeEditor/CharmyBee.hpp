#pragma once

#include <cstddef>
#include <cstdint>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <memory>
#include <optional>
#include <string>
#include <array>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_map>

#include "Editor/Panel/AssetPanel.hpp"
#include "Editor/Panel/InspectorPanel.hpp"
#include "Editor/Panel/ScenePanel.hpp"
#include "Editor/Panel/IEditorPanel.hpp"
#include "Editor/Scene.hpp"
#include "Editor/Tools/NavTool/NavRoute.hpp"
#include "Editor/Tools/NavTool/NavRenderMode.hpp"
#include "Editor/Tools/NavigationTool.hpp"
#include "Forest/ForestTypes.hpp"
#include "Managers/SlFile.hpp"
#include "Renderer/SlRenderer.hpp"
#include "SifParser.hpp"
#include <SlLib/Math/Vector.hpp>
#include "SlLib/Excel/ExcelData.hpp"
#include "SlLib/Resources/Database/SlResourceDatabase.hpp"
#include "SlLib/Resources/Scene/SeDefinitionNode.hpp"
#include "SlLib/Resources/Scene/SeGraphNode.hpp"
#include "SlLib/SumoTool/Siff/NavData/NavWaypoint.hpp"
#include "SlLib/SumoTool/Siff/Navigation.hpp"
#include "SlLib/SumoTool/Siff/LogicData.hpp"

struct ImGuiSettingsHandler;
struct ImGuiTextBuffer;
struct ImGuiContext;

namespace SeEditor {

namespace Graphics::ImGui {
class ImGuiController;
}

class CharmyBee
{
public:
    CharmyBee(std::string title, int width, int height, bool debugKeyInput = false);
    ~CharmyBee();

    void Run();

private:
    static void* SettingsReadOpen(ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name);
    static void SettingsReadLine(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line);
    static void SettingsWriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf);

    struct TreeNode
    {
        explicit TreeNode(std::string name, bool isFolder = false);
        std::string Name;
        bool IsFolder = false;
        SlLib::Resources::Scene::SeDefinitionNode* Association = nullptr;
        std::vector<std::unique_ptr<TreeNode>> Children;
    };

    struct BranchHierarchyNode
    {
        std::shared_ptr<Forest::SuBranch> Branch;
        std::vector<int> Children;
        bool Visible = true;
    };

    struct TreeHierarchy
    {
        std::string Name;
        std::vector<BranchHierarchyNode> Nodes;
        std::vector<int> Roots;
        bool Visible = true;
        int ForestIndex = -1;
        int TreeIndex = -1;
    };

    struct ForestHierarchy
    {
        std::string Name;
        std::vector<TreeHierarchy> Trees;
        bool Visible = true;
    };

    std::unique_ptr<TreeNode> _assetRoot;
    TreeNode* _selectedFolder = nullptr;

    std::string _title;
    int _width = 0;
    int _height = 0;
    std::unique_ptr<Graphics::ImGui::ImGuiController> _controller;
    std::unique_ptr<Editor::Panel::AssetPanel> _assetPanel;
    std::unique_ptr<Editor::Panel::ScenePanel> _scenePanel;
    std::unique_ptr<Editor::Panel::InspectorPanel> _inspectorPanel;
    bool _quickstart = true;
    Renderer::SlRenderer _renderer;
    std::vector<SlLib::Math::Vector3> _collisionVertices;
    std::vector<std::array<int, 3>> _collisionTriangles;
    bool _drawCollisionMesh = true;
    bool _drawForestBoxes = false;
    bool _drawForestMeshes = false;
    bool _drawTriggerBoxes = false;
    bool _drawNavigation = false;
    bool _drawNavigationWaypoints = true;
    float _navigationWaypointBoxSize = 2.0f;
    bool _drawLogic = false;
    bool _drawLogicTriggers = true;
    bool _drawLogicLocators = true;
    bool _drawLogicTriggerNormals = false;
    bool _drawLogicLocatorAxes = false;
    float _logicLocatorBoxSize = 2.0f;
    float _logicLocatorAxisSize = 4.0f;
    float _logicTriggerNormalSize = 6.0f;
    struct LogicTriggerGroup
    {
        int Hash = 0;
        std::string Name;
        int Count = 0;
        bool Visible = true;
        std::vector<int> Indices;
    };
    std::vector<LogicTriggerGroup> _logicTriggerGroups;
    std::unordered_map<int, std::size_t> _logicTriggerGroupIndex;
    bool _showStuffWindow = true;
    std::string _xpacStatus;
    std::filesystem::path _lastXpacPath;
    std::optional<std::filesystem::path> _stuffRootOverride;
    std::atomic<bool> _xpacBusy{false};
    std::atomic<std::size_t> _xpacProgress{0};
    std::atomic<std::size_t> _xpacTotal{0};
    std::atomic<std::size_t> _xpacConvertProgress{0};
    std::atomic<std::size_t> _xpacConvertTotal{0};
    std::string _xpacPopupText;
    std::atomic<bool> _xpacRepackBusy{false};
    std::string _xpacRepackPopupText;
    std::atomic<std::size_t> _xpacRepackProgress{0};
    std::atomic<std::size_t> _xpacRepackTotal{0};
    bool _confirmNukeStuff = false;
    std::atomic<bool> _unityExportBusy{false};
    std::string _unityExportStatus;
    std::mutex _unityExportMutex;
    std::unique_ptr<std::thread> _unityExportWorker;
    std::mutex _xpacMutex;
    std::unique_ptr<std::thread> _xpacWorker;
    struct XpacRepackEntry
    {
        std::filesystem::path Path;
        std::string Label;
        bool Selected = false;
    };
    bool _showXpacRepackPopup = false;
    std::filesystem::path _xpacRepackRoot;
    std::vector<XpacRepackEntry> _xpacRepackEntries;
    std::shared_ptr<SeEditor::Forest::ForestLibrary> _forestLibrary;
    using ForestMeshList = std::vector<Renderer::SlRenderer::ForestCpuMesh>;
    std::shared_ptr<SeEditor::Forest::ForestLibrary> _itemsForestLibrary;
    std::unordered_map<std::uint64_t, std::shared_ptr<ForestMeshList>> _itemsForestMeshesByForestTree;
    std::unordered_map<int, std::shared_ptr<ForestMeshList>> _itemsForestMeshesByTreeHash;
    std::vector<Renderer::SlRenderer::ForestCpuMesh> _logicLocatorMeshes;
    std::vector<bool> _logicLocatorHasMesh;
    struct ForestBoxLayer
    {
        std::string Name;
        bool Visible = true;
        bool HasBounds = false;
        SlLib::Math::Vector3 Min{};
        SlLib::Math::Vector3 Max{};
        std::vector<ForestBoxLayer> Children;
        std::size_t MeshStartIndex = 0;
        std::size_t MeshCount = 0;
        int ForestIndex = -1;
        int TreeIndex = -1;
    };
    bool _showForestHierarchyWindow = false;
    bool _showNavigationHierarchyWindow = false;
    std::vector<ForestBoxLayer> _forestBoxLayers;
    std::vector<ForestHierarchy> _forestHierarchy;
    std::vector<Renderer::SlRenderer::ForestCpuMesh> _allForestMeshes;
    struct NavigationLineEntry
    {
        int LineIndex = 0;
        std::string Name;
        bool Visible = true;
    };
    std::vector<NavigationLineEntry> _navigationLineEntries;
    float _orbitYaw = 0.6f;
    float _orbitPitch = 0.35f;
    float _orbitDistance = 10.0f;
    int _selectedChunk = -1;
    SlLib::Math::Vector3 _collisionCenter{0.0f, 0.0f, 0.0f};
    SlLib::Math::Vector3 _orbitTarget{0.0f, 0.5f, 0.0f};
    SlLib::Math::Vector3 _orbitOffset{0.0f, 0.0f, 0.0f};
    bool _debugKeyInput = false;
    std::array<bool, GLFW_KEY_LAST + 1> _glfwKeyStates{};
    std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> _glfwMouseButtonStates{};
    float _movementSpeed = 6.0f;
    std::string _sifFilePath;
    std::string _sifLoadMessage;
    std::vector<SifChunkInfo> _sifChunks;
    std::vector<SifChunkInfo> _sifGpuChunks;
    std::vector<std::uint8_t> _sifGpuRaw;
    std::vector<std::string> _sifHexDump;
    bool _sifFileCompressed = false;
    std::size_t _sifOriginalSize = 0;
    std::size_t _sifDecompressedSize = 0;
    std::string _sifParseError;
    std::optional<SlLib::Excel::ExcelData> _racerData;
    std::optional<SlLib::Excel::ExcelData> _trackData;
    SlLib::Resources::Database::SlResourceDatabase* _database = nullptr;
    SlLib::SumoTool::Siff::Navigation* _navigation = nullptr;
    std::unique_ptr<Editor::Tools::NavigationTool> _navigationTool;
    std::unique_ptr<SlLib::SumoTool::Siff::Navigation> _sifNavigation;
    std::unique_ptr<Editor::Tools::NavigationTool> _sifNavigationTool;
    std::unique_ptr<SlLib::SumoTool::Siff::LogicData> _sifLogic;
    std::vector<Editor::Tools::NavTool::NavRoute> _routes;
    Editor::Tools::NavTool::NavRoute* _selectedRoute = nullptr;
    SlLib::SumoTool::Siff::NavData::NavWaypoint* _selectedWaypoint = nullptr;
    int _selectedRacingLine = -1;
    int _selectedRacingLineSegment = -1;
    SeEditor::Editor::Tools::NavTool::NavRenderMode _navRenderMode = SeEditor::Editor::Tools::NavTool::NavRenderMode::Route;
    SlLib::Resources::Scene::SeGraphNode* _requestedNodeDeletion = nullptr;
    float _localSceneFrameWidth = 0.0f;
    float _localSceneFrameHeight = 0.0f;
    bool _mouseOrbitTracking = false;
    SlLib::Math::Vector2 _mouseOrbitLastPos{0.0f, 0.0f};
    bool _sceneViewHovered = false;
    std::size_t _totalForestMeshes = 0;

    void RenderMainDockWindow();
    void RenderStuffWindow();
    void RenderAssetView();
    void RenderSceneView();
    void RenderRacingLineEditor();
    void RenderPanelWindow(char const* title, Editor::Panel::IEditorPanel* panel);
    void OnLoad();
    void SetupNavigationRendering(SlLib::SumoTool::Siff::Navigation* navigation);
    void OnWorkspaceLoad();
    void LoadCollisionDebugGeometry();
    void LoadForestDebugGeometry();
    void LoadForestResources();
    void UpdateForestHierarchy();
    bool IsTreeVisible(std::size_t forestIdx, std::size_t treeIdx) const;
    bool IsBranchVisible(int forestIdx, int treeIdx, int branchIdx) const;
    void ApplyTreeVisibilityToLayers();
    void RenderForestHierarchyWindow();
    void RenderForestHierarchyList();
    bool RenderBranchNode(TreeHierarchy& tree, int index);
    void LoadNavigationResources();
    void LoadLogicResources();
    void LoadItemsForestResources();
    void BuildLogicLocatorMeshes();
    std::vector<std::uint8_t> BuildLogicChunkData() const;
    void ExportLogicRewrite();
    void ExportForestObj(std::filesystem::path const& outputPath,
                         std::string const& forestNameFilter);
    void RebuildForestBoxHierarchy();
    void UpdateForestBoxRenderer();
    void UpdateForestMeshRendering();
    void UpdateForestMeshVisibility();
    void LoadForestVisibility();
    void SaveForestVisibility() const;
    std::filesystem::path GetForestVisibilityPath() const;
    void UpdateNavigationLineVisibility();
    void UpdateNavigationDebugLines();
    void UpdateLogicDebugLines();
    void UpdateDebugLines();
    void UpdateTriggerPhantomBoxes();
    bool RenderForestBoxLayer(ForestBoxLayer& layer);
    void SetForestLayerVisibilityRecursive(ForestBoxLayer& layer, bool visible);
    void UpdateOrbitFromInput(float delta);
    void DrawNodeCreationMenu();
    void AddItemNode(std::string const& name, SlLib::Resources::Scene::SeDefinitionNode* definition = nullptr);
    void RenderSifViewer();
    void ExportSifToUnity();
    void UnpackXpac();
    void RepackXpac();
    void OpenSifFile();
    void LoadSifFile(std::filesystem::path const& path);
    void PollGlfwKeyInput();

    TreeNode* AddFolderNode(std::string const& path);
    void ResetAssetTree();
    std::vector<std::string> GetPathComponents(std::string const& value) const;
    std::filesystem::path GetStuffRoot() const;
    void RenderStuffTreeNode(std::filesystem::path const& path);
    void RenderStuffSifVirtualTree(std::filesystem::path const& root);
};

} // namespace SeEditor
