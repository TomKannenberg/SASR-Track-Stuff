#pragma once

#include "../../SlLib/Resources/Database/SlResourceDatabase.hpp"

namespace SlLib::Resources::Database {
class SlResourceDatabase;
}

namespace SeEditor::Editor {

class RacerSetup final
{
public:
    RacerSetup();

private:
    SlLib::Resources::Database::SlResourceDatabase _database;
};

} // namespace SeEditor::Editor
