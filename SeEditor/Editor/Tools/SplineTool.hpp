#pragma once

#include "EditorTool.hpp"

#include <SlLib/Math/Vector.hpp>
#include <SlLib/Resources/Scene/SeGraphNode.hpp>

#include <cstddef>
#include <vector>

namespace SlLib::Utilities {
struct MathUtils;
}

namespace SeEditor::Editor::Tools {

class SeInstanceSplineNode;

class SplineTool final : public EditorTool
{
public:
    void OnRender() override;
};

class SeInstanceSplineNode : public SlLib::Resources::Scene::SeGraphNode
{
public:
    SlLib::Math::Matrix4x4 WorldMatrix{};
    std::vector<float> Data;

    SlLib::Math::Vector3 ReadFloat3(std::size_t byteOffset) const;
};

} // namespace SeEditor::Editor::Tools
