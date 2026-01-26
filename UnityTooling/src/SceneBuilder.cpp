#include "UnityTooling/SceneBuilder.h"

#include <sstream>
#include <iomanip>

namespace UnityTooling {

SceneBuilder::SceneBuilder() = default;

void SceneBuilder::addDefaultSceneDocuments() {
    if (defaultSceneAdded_) {
        return;
    }

    addSceneDocument("!u!29", "Scene", "Scene:\n  m_ObjectHideFlags: 0\n  m_PVSData: \n  m_QueryMode: 1\n  m_PVSObjectsArray: []\n  m_PVSPortalsArray: []\n  m_ViewCellSize: 1.000000");
    addSceneDocument("!u!127", "GameManager", "GameManager:\n  m_ObjectHideFlags: 0");
    addSceneDocument("!u!157", "LightmapSettings", "LightmapSettings:\n  m_ObjectHideFlags: 0\n  m_LightProbeCloud: {fileID: 0}\n  m_Lightmaps: []\n  m_LightmapsMode: 1\n  m_BakedColorSpace: 0\n  m_UseDualLightmapsInForward: 0\n  m_LightmapEditorSettings:\n    m_Resolution: 50.000000\n    m_TextureWidth: 1024\n    m_TextureHeight: 1024\n    m_Quality: 0\n    m_Bounces: 1\n    m_FinalGatherRays: 1000\n    m_FinalGatherContrastThreshold: 0.050000\n    m_FinalGatherGradientThreshold: 0.000000\n    m_FinalGatherInterpolationPoints: 15\n    m_TextureCompression: 0\n    m_LockAtlas: 0\n  m_UseDualLightmapsInForward: 0");
    addSceneDocument("!u!196", "NavMeshSettings", "NavMeshSettings:\n  m_ObjectHideFlags: 0\n  m_BuildSettings:\n    cellSize: 0.200000\n    cellHeight: 0.100000\n    agentSlope: 45.000000\n    agentHeight: 1.800000\n    agentRadius: 0.400000\n    maxEdgeLength: 12\n    maxSimplificationError: 1.300000\n    regionMinSize: 8\n    regionMergeSize: 20\n    detailSampleDistance: 6.000000\n    detailSampleMaxError: 1.000000\n    accuratePlacement: 0\n  m_NavMesh: {fileID: 0}");

    defaultSceneAdded_ = true;
}

int SceneBuilder::addSceneDocument(const std::string& tag, const std::string& typeName, const std::string& body) {
    const int fileID = allocateFileID();
    documents_.push_back({tag, fileID, typeName, body});
    return fileID;
}

void SceneBuilder::addGameObject(const GameObjectDefinition& definition) {
    std::vector<int> componentIDs;
    for (const auto& component : definition.components) {
        componentIDs.push_back(addSceneDocument(component.tag, component.typeName, component.bodyBuilder(static_cast<int>(nextFileID_))));
    }

    addSceneDocument("!u!1", "GameObject", buildGameObjectBody(definition, componentIDs));
}

std::string SceneBuilder::serialize() const {
    std::ostringstream output;
    output << "%YAML 1.1\n";
    output << "%TAG !u! tag:unity3d.com,2011:\n";

    for (const auto& document : documents_) {
        output << "--- " << document.tag << " &" << document.fileID << "\n";
        output << document.body << "\n";
    }

    return output.str();
}

int SceneBuilder::allocateFileID() {
    return nextFileID_++;
}

std::string SceneBuilder::buildGameObjectBody(const GameObjectDefinition& definition, const std::vector<int>& componentIDs) const {
    std::ostringstream body;
    body << "GameObject:\n";
    body << "  m_ObjectHideFlags: 0\n";
    body << "  m_PrefabParentObject: {fileID: 0}\n";
    body << "  m_PrefabInternal: {fileID: 0}\n";
    body << "  importerVersion: 3\n";
    body << "  m_Component:\n";

    for (size_t i = 0; i < componentIDs.size(); ++i) {
        body << "  - " << stripTagPrefix(definition.components[i].tag) << ": {fileID: " << componentIDs[i] << "}\n";
    }

    body << "  m_Layer: " << definition.layer << "\n";
    body << "  m_Name: " << quoteIfNeeded(definition.name) << "\n";
    body << "  m_TagString: " << quoteIfNeeded(definition.tagString) << "\n";
    body << "  m_Icon: {fileID: 0}\n";
    body << "  m_NavMeshLayer: 0\n";
    body << "  m_StaticEditorFlags: 0\n";
    body << "  m_IsActive: " << (definition.isActive ? 1 : 0) << "\n";

    return body.str();
}

std::string SceneBuilder::stripTagPrefix(const std::string& tag) {
    const auto delimiter = tag.find_last_of('!');
    if (delimiter == std::string::npos) {
        return tag;
    }
    return tag.substr(delimiter + 1);
}

ComponentDefinition makeTransformComponent(Vector3 position, Quaternion rotation, Vector3 scale) {
    return ComponentDefinition{
        "!u!4",
        "Transform",
        [position, rotation, scale](int fileID) {
            std::ostringstream body;
            body << "Transform:\n";
            body << "  m_ObjectHideFlags: 0\n";
            body << "  m_PrefabParentObject: {fileID: 0}\n";
            body << "  m_PrefabInternal: {fileID: 0}\n";
            body << "  m_GameObject: {fileID: " << fileID << "}\n";
            body << "  m_LocalRotation: " << formatQuaternion(rotation) << "\n";
            body << "  m_LocalPosition: " << formatVector3(position) << "\n";
            body << "  m_LocalScale: " << formatVector3(scale) << "\n";
            body << "  m_Children: []\n";
            body << "  m_Father: {fileID: 0}\n";
            return body.str();
        }
    };
}

ComponentDefinition makeCameraComponent(float nearClip, float farClip, float fieldOfView, int clearFlags) {
    return ComponentDefinition{
        "!u!20",
        "Camera",
        [=](int fileID) {
            std::ostringstream body;
            body << "Camera:\n";
            body << "  m_ObjectHideFlags: 0\n";
            body << "  m_PrefabParentObject: {fileID: 0}\n";
            body << "  m_PrefabInternal: {fileID: 0}\n";
            body << "  m_GameObject: {fileID: " << fileID << "}\n";
            body << "  m_Enabled: 1\n";
            body << "  m_ClearFlags: " << clearFlags << "\n";
            body << "  m_BackGroundColor: {r: 0.0, g: 0.0, b: 0.0, a: 0.0}\n";
            body << "  near clip plane: " << nearClip << "\n";
            body << "  far clip plane: " << farClip << "\n";
            body << "  field of view: " << fieldOfView << "\n";
            body << "  orthographic: 0\n";
            body << "  orthographic size: 100.0\n";
            body << "  m_Depth: -1.0\n";
            body << "  m_CullingMask:\n";
            body << "    importerVersion: 2\n";
            body << "    m_Bits: 4294967295\n";
            body << "  m_RenderingPath: -1\n";
            body << "  m_TargetTexture: {fileID: 0}\n";
            body << "  m_HDR: 0\n";
            return body.str();
        }
    };
}

ComponentDefinition makeRendererComponent(const std::string& meshFileID, const std::string& materialGuid) {
    return ComponentDefinition{
        "!u!23",
        "Renderer",
        [=](int fileID) {
            std::ostringstream body;
            body << "Renderer:\n";
            body << "  m_ObjectHideFlags: 0\n";
            body << "  m_PrefabParentObject: {fileID: 0}\n";
            body << "  m_PrefabInternal: {fileID: 0}\n";
            body << "  m_GameObject: {fileID: " << fileID << "}\n";
            body << "  m_Enabled: 1\n";
            body << "  m_CastShadows: 1\n";
            body << "  m_ReceiveShadows: 1\n";
            body << "  m_LightmapIndex: 255\n";
            body << "  m_LightmapTilingOffset: {x: 1.000000, y: 1.000000, z: 0.000000, w: 0.000000}\n";
            body << "  m_Materials:\n";
            body << "  - {fileID: " << meshFileID << ", guid: " << materialGuid << ", type: 0}\n";
            body << "  m_SubsetIndices: \n";
            body << "  m_StaticBatchRoot: {fileID: 0}\n";
            body << "  m_LightProbeAnchor: {fileID: 0}\n";
            body << "  m_UseLightProbes: 0\n";
            body << "  m_ScaleInLightmap: 1.000000\n";
            return body.str();
        }
    };
}

ComponentDefinition makeBoxColliderComponent(Vector3 size, Vector3 center) {
    return ComponentDefinition{
        "!u!65",
        "BoxCollider",
        [=](int fileID) {
            std::ostringstream body;
            body << "BoxCollider:\n";
            body << "  m_ObjectHideFlags: 0\n";
            body << "  m_PrefabParentObject: {fileID: 0}\n";
            body << "  m_PrefabInternal: {fileID: 0}\n";
            body << "  m_GameObject: {fileID: " << fileID << "}\n";
            body << "  m_Material: {fileID: 0}\n";
            body << "  m_IsTrigger: 0\n";
            body << "  m_Enabled: 1\n";
            body << "  importerVersion: 2\n";
            body << "  m_Size: " << formatVector3(size) << "\n";
            body << "  m_Center: " << formatVector3(center) << "\n";
            return body.str();
        }
    };
}

std::string formatVector3(const Vector3& value) {
    std::ostringstream stream;
    stream << "{x: " << std::fixed << std::setprecision(6) << value.x
           << ", y: " << value.y
           << ", z: " << value.z << "}";
    return stream.str();
}

std::string formatQuaternion(const Quaternion& value) {
    std::ostringstream stream;
    stream << "{x: " << std::fixed << std::setprecision(6) << value.x
           << ", y: " << value.y
           << ", z: " << value.z
           << ", w: " << value.w << "}";
    return stream.str();
}

std::string quoteIfNeeded(const std::string& raw) {
    if (raw.find_first_of(" \t\n\r:") != std::string::npos) {
        std::ostringstream stream;
        stream << '"' << raw << '"';
        return stream.str();
    }
    return raw;
}

} // namespace UnityTooling
