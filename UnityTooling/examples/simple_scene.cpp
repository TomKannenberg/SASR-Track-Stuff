#include "UnityTooling/SceneBuilder.h"

#include <fstream>

int main() {
    UnityTooling::SceneBuilder builder;
    builder.addDefaultSceneDocuments();

    UnityTooling::GameObjectDefinition cube;
    cube.name = "Cube";
    cube.tagString = "Untagged";
    cube.components.push_back(UnityTooling::makeTransformComponent({0.0f, 0.5f, 0.0f}));
    cube.components.push_back(UnityTooling::makeRendererComponent());
    cube.components.push_back(UnityTooling::makeBoxColliderComponent());
    builder.addGameObject(cube);

    UnityTooling::GameObjectDefinition camera;
    camera.name = "Main Camera";
    camera.tagString = "MainCamera";
    camera.components.push_back(UnityTooling::makeTransformComponent({0.0f, 1.0f, -10.0f}));
    camera.components.push_back(UnityTooling::makeCameraComponent());
    builder.addGameObject(camera);

    const auto sceneData = builder.serialize();
    std::ofstream output("UnityTooling/examples/generated_scene.unity", std::ios::binary);
    output << sceneData;

    return 0;
}
