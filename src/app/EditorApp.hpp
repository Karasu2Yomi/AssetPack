#pragma once

#include <filesystem>
#include "EditorAppState.hpp"


namespace App {

class EditorApp {
public:
    void InitDefaultProject(EditorAppState& s);
    bool LoadDataFolder(EditorAppState& s, const std::filesystem::path& dataRoot);
    bool SaveAll(EditorAppState& s);
    static bool CreateLevel(EditorAppState& s, const std::string& id,
                            const std::string& name);
    static bool DuplicateSelectedLevel(EditorAppState& s);
    static void SelectLevel(EditorAppState& s, int index);
    static void SelectPreset(EditorAppState& s, int index);
    static bool BeginAddNode(EditorAppState& s);
    static bool PlaceNode(EditorAppState& s, int tileX, int tileY);
    void Tick(EditorAppState& s);
    void ClearSelection(EditorAppState& s);
};

} // namespace App
