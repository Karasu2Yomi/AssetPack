#pragma once

#include "assetpack_core/Model.hpp"
#include "assetpack_core/PuzzleProjectStore.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace App {

enum class SelectTarget { None, Level, Preset, MapNode };

struct CanvasState {
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    bool showGrid = true;
};

struct EditorAppState {
    AssetPackCore::PuzzleProject project;
    bool projectLoaded = false;

    bool requestClose = false;
    bool shouldExit = false;
    bool promptSaveBeforeClose = false;
    bool promptSaveBeforeLoad = false;
    std::filesystem::path pendingLoadPath;

    int selectedLevelIndex = -1;
    int selectedPresetIndex = -1;
    int selectedMapNodeIndex = -1;
    SelectTarget selectedTarget = SelectTarget::None;
    std::string selectedPresetForAdd;

    CanvasState canvas;
    bool isAddMode = false;

    int draggingNodeIndex = -1;
    bool draggingNode = false;
    bool panning = false;
    float panAnchorX = 0.0f;
    float panAnchorY = 0.0f;
    float panBaseX = 0.0f;
    float panBaseY = 0.0f;

    std::vector<AssetPackCore::ProjectIssue> issues;
    std::vector<std::string> console;

    [[nodiscard]] AssetPackCore::MapDocument* SelectedMap() {
        if (selectedLevelIndex < 0 ||
            selectedLevelIndex >= static_cast<int>(project.levels.size())) {
            return nullptr;
        }
        const auto it = project.maps.find(project.levels[selectedLevelIndex].map_path);
        return it == project.maps.end() || it->second.deleted ? nullptr : &it->second;
    }

    [[nodiscard]] const AssetPackCore::MapDocument* SelectedMap() const {
        if (selectedLevelIndex < 0 ||
            selectedLevelIndex >= static_cast<int>(project.levels.size())) {
            return nullptr;
        }
        const auto it = project.maps.find(project.levels[selectedLevelIndex].map_path);
        return it == project.maps.end() || it->second.deleted ? nullptr : &it->second;
    }
};

} // namespace App
