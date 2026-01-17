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
#include <filesystem>
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
    struct TreeNode
    {
        explicit TreeNode(std::string name, bool isFolder = false);
        std::string Name;
        bool IsFolder = false;
        SlLib::Resources::Scene::SeDefinitionNode* Association = nullptr;
        std::vector<std::unique_ptr<TreeNode>> Children;
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
    bool _requestedWorkspaceClose = false;
    bool _quickstart = true;
    Renderer::SlRenderer _renderer;
    std::vector<SlLib::Math::Vector3> _collisionVertices;
    std::vector<std::array<int, 3>> _collisionTriangles;
    bool _drawCollisionMesh = true;
    bool _drawForestBoxes = false;
    bool _drawForestMeshes = false;
    std::shared_ptr<SeEditor::Forest::ForestLibrary> _forestLibrary;
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
    };
    bool _showForestHierarchyWindow = false;
    bool _showHierarchyWindow = true;
    std::vector<ForestBoxLayer> _forestBoxLayers;
    std::vector<Renderer::SlRenderer::ForestCpuMesh> _allForestMeshes;
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
    void RenderWorkspace();
    void RenderHierarchy(bool definitions);
    void RenderHierarchyWindow();
    void RenderAssetView();
    void RenderSceneView();
    void RenderRacingLineEditor();
    void RenderProjectSelector();
    void RenderPanelWindow(char const* title, Editor::Panel::IEditorPanel* panel);
    void OnLoad();
    void SetupNavigationRendering();
    void OnWorkspaceLoad();
    void TriggerCloseWorkspace();
    void LoadCollisionDebugGeometry();
    void LoadForestDebugGeometry();
    void LoadForestResources();
    void ExportForestObj(std::filesystem::path const& outputPath,
                         std::string const& forestNameFilter);
    void RebuildForestBoxHierarchy();
    void UpdateForestBoxRenderer();
    void UpdateForestMeshRendering();
    void UpdateForestMeshVisibility();
    bool RenderForestBoxLayer(ForestBoxLayer& layer);
    void SetForestLayerVisibilityRecursive(ForestBoxLayer& layer, bool visible);
    void UpdateOrbitFromInput(float delta);
    void DrawNodeCreationMenu();
    void AddItemNode(std::string const& name, SlLib::Resources::Scene::SeDefinitionNode* definition = nullptr);
    void RenderSifViewer();
    void OpenSifFile();
    void ConvertSifWithWorkspace();
    void PollGlfwKeyInput();

    TreeNode* AddFolderNode(std::string const& path);
    void ResetAssetTree();
    std::vector<std::string> GetPathComponents(std::string const& value) const;
};

} // namespace SeEditor
