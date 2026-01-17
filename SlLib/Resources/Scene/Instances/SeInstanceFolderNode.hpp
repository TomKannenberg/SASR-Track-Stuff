#pragma once

#include "SlLib/Resources/Scene/SeInstanceNode.hpp"

namespace SlLib::Resources::Scene {

class SeInstanceFolderNode : public SeInstanceNode
{
public:
    SeInstanceFolderNode();
    ~SeInstanceFolderNode() override;
};

} // namespace SlLib::Resources::Scene
