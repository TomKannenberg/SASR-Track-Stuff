#include "IsDefinitionOfAttribute.hpp"

namespace SlLib::Attributes {

std::string toString(IsDefinitionOfAttribute const& attribute)
{
    return attribute.Type.name();
}

} // namespace SlLib::Attributes
