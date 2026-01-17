#include "SeNodeBase.hpp"

#include <chrono>
#include <sstream>

#include "SlLib/Utilities/SlUtil.hpp"

namespace SlLib::Resources::Scene {

SeNodeBase::SeNodeBase()
    : Tag{}
{
    ShortName = UidName;
    Scene = {};
    CleanName = {};
}

void SeNodeBase::SetNameWithTimestamp(std::string const& name)
{
    UidName = name;
    ShortName = name;
    Scene = name;
    CleanName = name;
    Uid = SlLib::Utilities::sumoHash(name);
}

void SeNodeBase::Load(Serialization::ResourceLoadContext& context)
{
    LoadInternal(context, static_cast<int>(context.Position));
}

void SeNodeBase::Save(Serialization::ResourceSaveContext&, Serialization::ISaveBuffer&)
{
}

int SeNodeBase::GetSizeForSerialization() const
{
    return 0x80;
}

int SeNodeBase::LoadInternal(Serialization::ResourceLoadContext&, int offset)
{
    return offset + 0x40;
}

} // namespace SlLib::Resources::Scene
