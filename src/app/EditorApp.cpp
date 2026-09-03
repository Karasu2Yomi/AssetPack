#include "EditorApp.hpp"
#include "assetpack_core/PuzzleProjectStore.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace {

using App::EditorAppState;
using namespace AssetPackCore;

bool EditError(EditorAppState& s, const std::string& message) {
    s.console.push_back("エラー: " + message);
    return false;
}

bool IsLevelId(const std::string& id) {
    if (id.empty() || id.size() > 64 || id.front() < 'a' || id.front() > 'z' ||
        id.back() == '_') {
        return false;
    }
    bool underscore = false;
    for (const char ch : id) {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_') ||
            (underscore && ch == '_')) {
            return false;
        }
        underscore = ch == '_';
    }
    return true;
}

std::string WithSuffix(const std::string& base, const std::string& suffix) {
    std::string stem = base.substr(0, 64 - suffix.size());
    while (!stem.empty() && stem.back() == '_') stem.pop_back();
    return stem + suffix;
}

std::string UniqueLevelId(const PuzzleProject& project, const std::string& base) {
    for (int n = 0; n < 10000; ++n) {
        const auto candidate = n == 0 ? base : WithSuffix(base, "_" + std::to_string(n));
        if (std::none_of(project.levels.begin(), project.levels.end(),
                         [&](const LevelRow& level) { return level.level_id == candidate; })) {
            return candidate;
        }
    }
    return {};
}

std::string PathKey(const std::filesystem::path& path) {
    std::string key = path.lexically_normal().generic_string();
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
#endif
    return key;
}

bool UniqueMapPath(EditorAppState& s, const std::string& id, std::string& result) {
    std::error_code ec;
    const auto resourceRoot = std::filesystem::absolute(s.project.resourceRoot, ec);
    if (ec) return EditError(s, "リソースフォルダを確認できません。");
    const auto dataRoot = std::filesystem::absolute(s.project.dataRoot, ec);
    if (ec || !std::filesystem::is_directory(dataRoot, ec) || ec) {
        return EditError(s, "データフォルダを確認できません。");
    }
    const auto relativeDir = (dataRoot / "maps").lexically_normal().lexically_relative(
        resourceRoot.lexically_normal());
    if (relativeDir.empty() || relativeDir.is_absolute() ||
        *relativeDir.begin() == "..") {
        return EditError(s, "マップはリソースフォルダ内に作成してください。");
    }
    for (int n = 0; n < 10000; ++n) {
        const std::string stem = n == 0 ? id : id + "_" + std::to_string(n);
        const auto candidate = relativeDir / (stem + ".csv");
        const auto key = PathKey(candidate);
        const bool inMemory = std::any_of(s.project.maps.begin(), s.project.maps.end(),
            [&](const auto& entry) { return PathKey(entry.first) == key; }) ||
            std::any_of(s.project.levels.begin(), s.project.levels.end(),
            [&](const LevelRow& level) { return PathKey(level.map_path) == key; });
        if (inMemory) continue;
        const auto status = std::filesystem::symlink_status(resourceRoot / candidate, ec);
        if (ec && ec != std::errc::no_such_file_or_directory) {
            return EditError(s, "マップの保存先を確認できません: " + ec.message());
        }
        ec.clear();
        if (std::filesystem::exists(status)) continue;
        result = candidate.generic_string();
        return true;
    }
    return EditError(s, "使用できるマップ名が見つかりません。");
}

void ResetMapInteraction(EditorAppState& s) {
    s.selectedMapNodeIndex = -1;
    s.selectedPresetForAdd.clear();
    s.isAddMode = false;
    s.draggingNode = false;
    s.draggingNodeIndex = -1;
    s.panning = false;
}

bool AppendDraftLevel(EditorAppState& s, LevelRow level, MapDocument map) {
    if (!UniqueMapPath(s, level.level_id, level.map_path)) return false;
    level.line = 0;
    map.map_path = level.map_path;
    map.state = MapDocumentState::NewDraft;
    map.dirty = true;
    map.deleted = false;
    s.project.maps.emplace(level.map_path, std::move(map));
    s.project.mapLoadOrder.push_back(level.map_path);
    s.project.levels.push_back(std::move(level));
    s.project.levelsDirty = true;
    App::EditorApp::SelectLevel(s, static_cast<int>(s.project.levels.size()) - 1);
    return true;
}

}  // namespace

namespace App {

void EditorApp::InitDefaultProject(EditorAppState& s) {
    s.projectLoaded = false;
    s.shouldExit = false;
    s.requestClose = false;
    s.promptSaveBeforeClose = false;
    s.promptSaveBeforeLoad = false;
    s.pendingLoadPath.clear();
    s.selectedTarget = SelectTarget::None;
    s.selectedLevelIndex = -1;
    s.selectedPresetIndex = -1;
    s.selectedMapNodeIndex = -1;
    s.selectedPresetForAdd.clear();
    s.canvas.zoom = 1.0f;
    s.canvas.panX = 0.0f;
    s.canvas.panY = 0.0f;
    s.canvas.showGrid = true;
    s.isAddMode = false;
    s.draggingNode = false;
    s.draggingNodeIndex = -1;
    s.panning = false;
    s.panAnchorX = 0.0f;
    s.panAnchorY = 0.0f;
    s.panBaseX = 0.0f;
    s.panBaseY = 0.0f;
    s.project = {};
    s.issues.clear();
    s.console.clear();
}

bool EditorApp::LoadDataFolder(EditorAppState& s,
                               const std::filesystem::path& dataRoot) {
    if (AssetPackCore::PuzzleProjectStore::LoadDataFolder(dataRoot, s.project, s.issues)) {
        s.projectLoaded = true;
        s.selectedLevelIndex = -1;
        s.selectedPresetIndex = -1;
        s.selectedMapNodeIndex = -1;
        s.selectedTarget = SelectTarget::None;
        s.selectedPresetForAdd.clear();
        s.isAddMode = false;
        s.draggingNode = false;
        s.draggingNodeIndex = -1;
        s.panning = false;
        s.pendingLoadPath.clear();
        s.promptSaveBeforeLoad = false;
        return true;
    }
    return false;
}

bool EditorApp::SaveAll(EditorAppState& s) {
    if (!s.projectLoaded) {
        return false;
    }
    return AssetPackCore::PuzzleProjectStore::SaveAll(s.project, s.issues);
}

bool EditorApp::CreateLevel(EditorAppState& s, const std::string& id,
                            const std::string& name) {
    if (!s.projectLoaded || s.project.dataRoot.empty() || s.project.resourceRoot.empty()) {
        return EditError(s, "先にデータフォルダを開いてください。");
    }
    if (!IsLevelId(id)) {
        return EditError(s, "level_id は64文字以内の lower_snake_case で入力してください。");
    }
    if (name.empty()) return EditError(s, "level_name を入力してください。");
    LevelRow level;
    level.level_id = UniqueLevelId(s.project, id);
    if (level.level_id.empty()) return EditError(s, "使用できるレベルIDが見つかりません。");
    level.level_name = name;
    return AppendDraftLevel(s, std::move(level), {});
}

bool EditorApp::DuplicateSelectedLevel(EditorAppState& s) {
    const auto* sourceMap = s.SelectedMap();
    if (!s.projectLoaded || !sourceMap) {
        return EditError(s, "複製するレベルとマップを選択してください。");
    }
    // Copy before appending: growing levels invalidates references to its rows.
    LevelRow level = s.project.levels[s.selectedLevelIndex];
    MapDocument map = *sourceMap;
    level.level_id = UniqueLevelId(s.project, WithSuffix(level.level_id, "_copy"));
    if (level.level_id.empty()) return EditError(s, "使用できるレベルIDが見つかりません。");
    return AppendDraftLevel(s, std::move(level), std::move(map));
}

void EditorApp::SelectLevel(EditorAppState& s, int index) {
    ResetMapInteraction(s);
    s.selectedLevelIndex = index >= 0 && index < static_cast<int>(s.project.levels.size())
                               ? index : -1;
    s.selectedTarget = s.selectedLevelIndex >= 0 ? SelectTarget::Level : SelectTarget::None;
}

void EditorApp::SelectPreset(EditorAppState& s, int index) {
    ResetMapInteraction(s);
    s.selectedPresetIndex = index >= 0 && index < static_cast<int>(s.project.presets.size())
                                ? index : -1;
    s.selectedTarget = s.selectedPresetIndex >= 0 ? SelectTarget::Preset : SelectTarget::None;
}

bool EditorApp::BeginAddNode(EditorAppState& s) {
    if (!s.projectLoaded || !s.SelectedMap() || s.selectedPresetIndex < 0 ||
        s.selectedPresetIndex >= static_cast<int>(s.project.presets.size())) {
        return EditError(s, "追加先のレベルとノードひな形を選択してください。");
    }
    ResetMapInteraction(s);
    s.selectedPresetForAdd = s.project.presets[s.selectedPresetIndex].preset_id;
    s.isAddMode = true;
    return true;
}

bool EditorApp::PlaceNode(EditorAppState& s, int tileX, int tileY) {
    auto* map = s.SelectedMap();
    if (!s.projectLoaded || !map || !s.isAddMode || tileX < 0 || tileX >= 80 ||
        tileY < 0 || tileY >= 45) {
        return false;
    }
    const auto preset = std::find_if(s.project.presets.begin(), s.project.presets.end(),
        [&](const NodePresetRow& row) { return row.preset_id == s.selectedPresetForAdd; });
    if (preset == s.project.presets.end()) {
        ResetMapInteraction(s);
        return EditError(s, "追加するノードひな形が見つかりません。");
    }
    MapNodeRow node;
    node.source_preset_id = preset->preset_id;
    for (int n = 1; n < 10000; ++n) {
        const auto id = WithSuffix(preset->preset_id, "_" + std::to_string(n));
        if (std::none_of(map->nodes.begin(), map->nodes.end(),
                        [&](const MapNodeRow& row) { return row.instance_id == id; })) {
            node.instance_id = id;
            break;
        }
    }
    if (node.instance_id.empty()) return EditError(s, "使用できるノードIDが見つかりません。");
    node.tile_x = {true, std::to_string(tileX)};
    node.tile_y = {true, std::to_string(tileY)};
    map->nodes.push_back(std::move(node));
    map->dirty = true;
    ResetMapInteraction(s);
    s.selectedTarget = SelectTarget::MapNode;
    s.selectedMapNodeIndex = static_cast<int>(map->nodes.size()) - 1;
    return true;
}

void EditorApp::ClearSelection(EditorAppState& s) {
    ResetMapInteraction(s);
    s.selectedLevelIndex = -1;
    s.selectedPresetIndex = -1;
    s.selectedMapNodeIndex = -1;
    s.selectedTarget = SelectTarget::None;
}

void EditorApp::Tick(EditorAppState&) {
    // すべての操作は UI 側のボタン/ドラッグで処理する。
}

}  // namespace App
