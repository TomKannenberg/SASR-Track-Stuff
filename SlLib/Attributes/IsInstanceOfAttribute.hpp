#pragma once

#include <string>
#include <typeindex>
#include <type_traits>

namespace SlLib::Attributes {

struct IsInstanceOfAttribute
{
    explicit IsInstanceOfAttribute(std::type_index typeIndex) : Type(typeIndex) {}

    template <typename T>
    explicit IsInstanceOfAttribute(std::type_identity_t<T>* = nullptr)
        : Type(typeid(T))
    {
    }

    std::type_index Type;
};

std::string toString(IsInstanceOfAttribute const&);

} // namespace SlLib::Attributes
