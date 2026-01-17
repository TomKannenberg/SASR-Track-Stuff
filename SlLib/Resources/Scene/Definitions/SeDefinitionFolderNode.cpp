#include "SeDefinitionFolderNode.hpp"

namespace SlLib::Resources::Scene {

SeDefinitionFolderNode& SeDefinitionFolderNode::Default()
{
    static SeDefinitionFolderNode instance;
    static bool initialized = false;
    if (!initialized) {
        instance.UidName = "DefaultFolder";
        initialized = true;
    }
    return instance;
}

int SeDefinitionFolderNode::GetSizeForSerialization() const
{
    return 0xd0;
}

} // namespace SlLib::Resources::Scene
