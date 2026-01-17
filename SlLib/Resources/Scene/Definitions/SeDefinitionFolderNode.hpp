#pragma once

#include "SlLib/Resources/Scene/SeDefinitionTransformNode.hpp"

namespace SlLib::Resources::Scene {

class SeDefinitionFolderNode : public SeDefinitionTransformNode
{
public:
    static SeDefinitionFolderNode& Default();

    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene
