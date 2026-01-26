#pragma once

#include <functional>
#include <string>
#include <vector>

namespace UnityTooling {

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct ComponentDefinition {
    std::string tag;
    std::string typeName;
    std::function<std::string(int)> bodyBuilder;
};

struct GameObjectDefinition {
    std::string name;
    std::string tagString = "Untagged";
    int layer = 0;
    bool isActive = true;
    std::vector<ComponentDefinition> components;
};

class SceneBuilder {
public:
    SceneBuilder();

    void addDefaultSceneDocuments();
    int addSceneDocument(const std::string& tag, const std::string& typeName, const std::string& body);
    void addGameObject(const GameObjectDefinition& definition);
    std::string serialize() const;

private:
    struct SceneDocument {
        std::string tag;
        int fileID;
        std::string typeName;
        std::string body;
    };

    int allocateFileID();
    std::string buildGameObjectBody(const GameObjectDefinition& definition, const std::vector<int>& componentIDs) const;
    static std::string stripTagPrefix(const std::string& tag);

    std::vector<SceneDocument> documents_;
    int nextFileID_ = 1;
    bool defaultSceneAdded_ = false;
};

ComponentDefinition makeTransformComponent(Vector3 position = {}, Quaternion rotation = {}, Vector3 scale = {1.0f, 1.0f, 1.0f});
ComponentDefinition makeCameraComponent(float nearClip = 0.3f, float farClip = 1000.0f, float fieldOfView = 60.0f, int clearFlags = 1);
ComponentDefinition makeRendererComponent(const std::string& meshFileID = "10202",
                                          const std::string& materialGuid = "0000000000000000e000000000000000");
ComponentDefinition makeBoxColliderComponent(Vector3 size = {1.0f, 1.0f, 1.0f}, Vector3 center = {});

std::string formatVector3(const Vector3& value);
std::string formatQuaternion(const Quaternion& value);
std::string quoteIfNeeded(const std::string& raw);

} // namespace UnityTooling
