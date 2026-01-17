#include "SeGraphNode.hpp"

namespace SlLib::Resources::Scene {

SeGraphNode::~SeGraphNode()
{
    SetParent(nullptr);
    FirstChild = nullptr;
    PrevSibling = nullptr;
    NextSibling = nullptr;
}

void SeGraphNode::SetParent(SeGraphNode* parent)
{
    if (Parent == parent) {
        return;
    }

    if (Parent != nullptr) {
        if (Parent->FirstChild == this) {
            Parent->FirstChild = NextSibling;
        }
        if (PrevSibling != nullptr) {
            PrevSibling->NextSibling = NextSibling;
        }
        if (NextSibling != nullptr) {
            NextSibling->PrevSibling = PrevSibling;
        }
        PrevSibling = nullptr;
        NextSibling = nullptr;
    }

    Parent = parent;
    if (parent != nullptr) {
        if (parent->FirstChild == nullptr) {
            parent->FirstChild = this;
        } else {
            SeGraphNode* child = parent->FirstChild;
            while (child->NextSibling != nullptr) {
                child = child->NextSibling;
            }
            child->NextSibling = this;
            PrevSibling = child;
        }
    }
}

} // namespace SlLib::Resources::Scene
