#pragma once

#include "SlLib/Enums/InstanceTimeStep.hpp"
#include "SlLib/Resources/Scene/SeDefinitionNode.hpp"
#include "SeGraphNode.hpp"

#include <type_traits>
#include <typeinfo>

namespace SlLib::Resources::Scene {

class SeInstanceNode : public SeGraphNode
{
public:
    SeInstanceNode();
    ~SeInstanceNode() override;

    SeDefinitionNode* Definition() const;
    void SetDefinition(SeDefinitionNode* definition);

    float LocalTimeScale = 1.0f;
    float LocalTime = 0.0f;
    int SceneGraphFlags = 16;

    SlLib::Enums::InstanceTimeStep TimeStep() const;
    void SetTimeStep(SlLib::Enums::InstanceTimeStep value);

    int LoadInternal(Serialization::ResourceLoadContext& context, int offset) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;

    template <typename T, typename = std::enable_if_t<std::is_base_of_v<SeInstanceNode, T>>>
    static T* CreateObject();

    template <typename T, typename = std::enable_if_t<std::is_base_of_v<SeInstanceNode, T>>>
    static T* CreateObject(SeDefinitionNode* definition);

private:
    SeDefinitionNode* _definition = nullptr;
};

} // namespace SlLib::Resources::Scene

namespace SlLib::Resources::Scene {

template <typename T, typename>
T* SeInstanceNode::CreateObject()
{
    T* node = new T();
    node->SetNameWithTimestamp(typeid(T).name());
    return node;
}

template <typename T, typename>
T* SeInstanceNode::CreateObject(SeDefinitionNode* definition)
{
    T* node = CreateObject<T>();
    if (definition != nullptr) {
        node->SetDefinition(definition);
        node->SetNameWithTimestamp(definition->UidName);
    }
    return node;
}

} // namespace SlLib::Resources::Scene
