#pragma once

#include "SeNodeBase.hpp"

namespace SlLib::Resources::Scene {

class SeGraphNode : public SeNodeBase
{
public:
    SeGraphNode* Parent = nullptr;
    SeGraphNode* FirstChild = nullptr;
    SeGraphNode* PrevSibling = nullptr;
    SeGraphNode* NextSibling = nullptr;

    virtual ~SeGraphNode();

    void SetParent(SeGraphNode* parent);
};

} // namespace SlLib::Resources::Scene
