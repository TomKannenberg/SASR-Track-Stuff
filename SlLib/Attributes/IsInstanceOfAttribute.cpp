#include "IsInstanceOfAttribute.hpp"

namespace SlLib::Attributes {

std::string toString(IsInstanceOfAttribute const& attribute)
{
    return attribute.Type.name();
}

} // namespace SlLib::Attributes
