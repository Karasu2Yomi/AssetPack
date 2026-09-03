#include "UiProject.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <string>
#include <unordered_set>
#include <vector>

#include "app/EditorApp.hpp"
#include "app/Logger.hpp"
#include "assetpack_core/PuzzleProjectStore.hpp"
#include "platform/WinFileDialog.hpp"

namespace {

using namespace AssetPackCore;
using App::EditorAppState;
using App::SelectTarget;

static bool IsValidId(const std::string& text) {
    if (text.empty() || text.size() > 128) {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        const bool lower = ch >= 'a' && ch <= 'z';
        const bool digit = ch >= '0' && ch <= '9';
        const bool underscore = ch == '_';
        if (i == 0) {
            if (!(lower || underscore)) return false;
        } else if (!(lower || digit || underscore)) {
            return false;
        }
        if (underscore && i + 1 < text.size() && text[i + 1] == '_') return false;
    }
    return text.back() != '_';
}

static std::string NormalizePath(std::string path) {
    for (char& ch : path) {
        if (ch == '\\') ch = '/';
    }
    while (!path.empty() && path.back() == '/') path.pop_back();
    while (!path.empty() && path[0] == '/') path.erase(path.begin());
    return path;
}

static std::string PresetUsageHint(const EditorAppState& s, const std::string& presetId) {
    int count = 0;
    std::vector<std::string> samples;
    for (const auto& level : s.project.levels) {
        auto mapIt = s.project.maps.find(level.map_path);
        if (mapIt == s.project.maps.end()) continue;
        for (const auto& node : mapIt->second.nodes) {
            if (node.source_preset_id == presetId) {
                ++count;
                if (samples.size() < 3) {
                    samples.push_back(level.level_id + "/" + node.instance_id);
                }
            }
        }
    }
    if (count == 0) return {};
    std::string out = "参照: " + std::to_string(count);
    if (!samples.empty()) {
        out += " (";
        for (std::size_t i = 0; i < samples.size(); ++i) {
            if (i > 0) out += ", ";
            out += samples[i];
        }
        if (count > static_cast<int>(samples.size())) {
            out += ", ...";
        }
        out += ")";
    }
    return out;
}

static bool IsPlacedNode(const MapNodeRow& node) {
    return !node.tile_x.value.empty() && !node.tile_y.value.empty() &&
           node.tile_x.overridden && node.tile_y.overridden;
}

static bool NodeHasInstance(const MapDocument& map, const std::string& instanceId,
                           int exceptIndex = -1) {
    for (int i = 0; i < static_cast<int>(map.nodes.size()); ++i) {
        if (i == exceptIndex) continue;
        if (map.nodes[i].instance_id == instanceId) {
            return true;
        }
    }
    return false;
}

static std::string MakeUniquePresetId(const EditorAppState& s, const std::string& base) {
    std::unordered_set<std::string> used;
    for (const auto& p : s.project.presets) used.insert(p.preset_id);
    if (!used.count(base)) return base;
    for (int i = 1; i < 9999; ++i) {
        const std::string candidate = base + "_" + std::to_string(i);
        if (!used.count(candidate)) return candidate;
    }
    return base + "_9999";
}

static std::string MakeUniqueInstanceId(const MapDocument& map,
                                       const std::string& presetId) {
    for (int i = 1; i < 9999; ++i) {
        const std::string candidate = presetId + "_" + std::to_string(i);
        if (!NodeHasInstance(map, candidate)) return candidate;
    }
    return presetId + "_9999";
}

static bool RemoveLevel(EditorAppState& s, int index) {
    if (index < 0 || index >= (int)s.project.levels.size()) return false;
    const std::string oldId = s.project.levels[index].level_id;
    s.project.levels.erase(s.project.levels.begin() + index);

    for (auto& l : s.project.levels) {
        if (l.next_level_id == oldId) l.next_level_id.clear();
    }
    s.project.levelsDirty = true;
    App::EditorApp::SelectLevel(
        s, std::min(index, static_cast<int>(s.project.levels.size()) - 1));
    return true;
}

static bool IsPresetUsed(const EditorAppState& s, const std::string& presetId) {
    for (const auto& mapPair : s.project.maps) {
        for (const auto& node : mapPair.second.nodes) {
            if (node.source_preset_id == presetId) return true;
        }
    }
    return false;
}

static void DrawLevelSection(EditorAppState& s) {
    ImGui::SeparatorText("レベル");

    static std::string newId;
    static std::string newName;
    ImGui::InputText("新規ID（level_id）", &newId);
    ImGui::InputText("新規名（level_name）", &newName);

    if (ImGui::Button("追加")) {
        if (App::EditorApp::CreateLevel(s, NormalizePath(newId), newName)) {
            newId.clear();
            newName.clear();
            App::Log(s, "レベルを追加しました。レベルを選択済みです。");
        }
    }
    ImGui::SameLine();

    if (ImGui::Button("複製") && App::EditorApp::DuplicateSelectedLevel(s)) {
        App::Log(s, "レベルを複製しました（map は別ファイル）。");
    }
    ImGui::SameLine();

    if (ImGui::Button("上へ") && s.selectedLevelIndex > 0) {
        std::swap(s.project.levels[s.selectedLevelIndex],
                  s.project.levels[s.selectedLevelIndex - 1]);
        s.selectedLevelIndex -= 1;
        s.project.levelsDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("下へ") && s.selectedLevelIndex >= 0 &&
        s.selectedLevelIndex + 1 < (int)s.project.levels.size()) {
        std::swap(s.project.levels[s.selectedLevelIndex],
                  s.project.levels[s.selectedLevelIndex + 1]);
        s.selectedLevelIndex += 1;
        s.project.levelsDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("削除") && s.selectedLevelIndex >= 0) {
        const auto deleted = s.project.levels[s.selectedLevelIndex].level_id;
        if (RemoveLevel(s, s.selectedLevelIndex)) {
            App::Log(s, "レベル削除: " + deleted + "（map は削除しません）");
        }
    }

    ImGui::BeginChild("level_list", ImVec2(0, 180), true);
    for (int i = 0; i < (int)s.project.levels.size(); ++i) {
        const auto& level = s.project.levels[i];
        const bool sel = (s.selectedLevelIndex == i);
        if (ImGui::Selectable(level.level_id.c_str(), sel)) {
            App::EditorApp::SelectLevel(s, i);
        }
    }
    ImGui::EndChild();

    if (s.selectedLevelIndex >= 0) {
        const auto& level = s.project.levels[s.selectedLevelIndex];
        bool shared = false;
        for (const auto& other : s.project.levels) {
            if (&other != &level && other.map_path == level.map_path) {
                shared = true;
                break;
            }
        }
        if (shared) ImGui::TextUnformatted("同一 map_path を共有しています（共有マップ）");
        ImGui::Text("選択中マップ: %s", level.map_path.c_str());
    }
}

static void DrawPresetSection(EditorAppState& s) {
    ImGui::SeparatorText("ノードひな形");
    static std::string newPresetId;
    static int newPresetType = 0;

    ImGui::InputText("新規 preset_id", &newPresetId);
    const char* types[] = {"root", "follow", "end", "dead"};
    ImGui::Combo("type", &newPresetType, types, 4);

    if (ImGui::Button("追加")) {
        const std::string id = MakeUniquePresetId(s, NormalizePath(newPresetId));
        if (!id.empty() && IsValidId(id)) {
            NodePresetRow p;
            p.preset_id = id;
            p.node_type = types[newPresetType];
            p.texture_path.clear();
            p.width_tiles = "1";
            p.height_tiles = "1";
            p.display_name.clear();
            p.max_incoming.clear();
            p.max_outgoing.clear();
            p.max_outgoing_length.clear();
            s.project.presets.push_back(p);
            s.project.presetsDirty = true;
            newPresetId.clear();
        } else {
            App::Log(s, "エラー: preset_id は lower_snake_case で入力してください。");
        }
    }
    ImGui::SameLine();

    if (ImGui::Button("複製") && s.selectedPresetIndex >= 0 &&
        s.selectedPresetIndex < (int)s.project.presets.size()) {
        auto dup = s.project.presets[s.selectedPresetIndex];
        dup.preset_id = MakeUniquePresetId(s, dup.preset_id + "_copy");
        s.project.presets.push_back(dup);
        s.project.presetsDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("上へ") && s.selectedPresetIndex > 0) {
        std::swap(s.project.presets[s.selectedPresetIndex],
                  s.project.presets[s.selectedPresetIndex - 1]);
        s.selectedPresetIndex--;
        s.project.presetsDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("下へ") && s.selectedPresetIndex >= 0 &&
        s.selectedPresetIndex + 1 < (int)s.project.presets.size()) {
        std::swap(s.project.presets[s.selectedPresetIndex],
                  s.project.presets[s.selectedPresetIndex + 1]);
        s.selectedPresetIndex++;
        s.project.presetsDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("削除") && s.selectedPresetIndex >= 0) {
        const std::string id = s.project.presets[s.selectedPresetIndex].preset_id;
        if (IsPresetUsed(s, id)) {
            App::Log(s,
                     "このノードひな形はマップから参照中（" +
                         PresetUsageHint(s, id) + "）ため削除できません。");
        } else {
            s.project.presets.erase(s.project.presets.begin() + s.selectedPresetIndex);
            s.project.presetsDirty = true;
            App::EditorApp::SelectPreset(s, -1);
            App::Log(s, "ノードひな形を削除しました。");
        }
    }

    ImGui::BeginChild("preset_list", ImVec2(0, 160), true);
    for (int i = 0; i < (int)s.project.presets.size(); ++i) {
        const auto& p = s.project.presets[i];
        const std::string usage = PresetUsageHint(s, p.preset_id);
        const std::string label =
            p.preset_id + " (" + p.node_type + ")" +
            (usage.empty() ? "" : " / " + usage);
        const bool sel = (s.selectedPresetIndex == i &&
                          s.selectedTarget == SelectTarget::Preset);
        if (ImGui::Selectable(label.c_str(), sel)) {
            App::EditorApp::SelectPreset(s, i);
        }
    }
    ImGui::EndChild();
}

static void DrawMapNodeSection(EditorAppState& s) {
    ImGui::SeparatorText("マップノード");
    if (s.selectedLevelIndex < 0 || s.selectedLevelIndex >= (int)s.project.levels.size()) {
        ImGui::TextUnformatted("レベルを選択してください。");
        return;
    }

    const auto mapPath = s.project.levels[s.selectedLevelIndex].map_path;
    auto* map = s.SelectedMap();
    if (!map) {
        ImGui::TextUnformatted("マップを取得できません。データフォルダを再読込してください。");
        return;
    }

    ImGui::Text("対象マップ: %s", mapPath.c_str());
    if (map->state == MapDocumentState::NewDraft) {
        ImGui::TextWrapped("新規マップ（未保存）。保存するまでファイルは作成されません。");
    } else if (map->state == MapDocumentState::MissingFileDraft) {
        ImGui::TextWrapped("参照先ファイルがありません。編集内容は保存時に新しいファイルへ書き込みます。");
    }
    const bool hasPreset = s.selectedPresetIndex >= 0 &&
                           s.selectedPresetIndex < (int)s.project.presets.size();
    if (hasPreset) {
        ImGui::Text("追加するひな形: %s",
                    s.project.presets[s.selectedPresetIndex].preset_id.c_str());
    }
    ImGui::BeginDisabled(!hasPreset);
    if (ImGui::Button("マップへ追加") && App::EditorApp::BeginAddNode(s)) {
        App::Log(s, "キャンバスをクリックしてノードを追加してください。");
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("選択解除")) {
        App::EditorApp::SelectLevel(s, s.selectedLevelIndex);
        s.selectedTarget = SelectTarget::None;
    }
    if (!hasPreset) {
        ImGui::TextUnformatted("ノードひな形を選択してからマップへ追加してください。");
    }

    ImGui::Text("ノード数: %d", static_cast<int>(map->nodes.size()));

    ImGui::BeginChild("map_node_list", ImVec2(0, 180), true);
    for (int i = 0; i < (int)map->nodes.size(); ++i) {
        const auto& node = map->nodes[i];
        const bool placed = IsPlacedNode(node);
        const std::string label =
            std::string(placed ? "配置" : "未配置") + " / " +
            node.instance_id + " / " + node.source_preset_id;
        const bool sel = (s.selectedMapNodeIndex == i &&
                          s.selectedTarget == SelectTarget::MapNode);
        if (ImGui::Selectable(label.c_str(), sel)) {
            s.selectedTarget = SelectTarget::MapNode;
            s.selectedMapNodeIndex = i;
            s.isAddMode = false;
            s.selectedPresetForAdd.clear();
            s.draggingNode = false;
            s.draggingNodeIndex = -1;
        }
    }
    ImGui::EndChild();

    if (s.selectedMapNodeIndex < 0 || s.selectedMapNodeIndex >= (int)map->nodes.size()) {
        s.selectedMapNodeIndex = -1;
    }

    if (s.selectedMapNodeIndex >= 0) {
        if (ImGui::Button("複製")) {
            auto dup = map->nodes[s.selectedMapNodeIndex];
            const std::string base = dup.instance_id.empty()
                                        ? "node"
                                        : dup.instance_id;
            dup.instance_id = MakeUniqueInstanceId(*map, base);
            map->nodes.insert(map->nodes.begin() + s.selectedMapNodeIndex + 1, dup);
            map->dirty = true;
            s.selectedMapNodeIndex += 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("削除")) {
            map->nodes.erase(map->nodes.begin() + s.selectedMapNodeIndex);
            map->dirty = true;
            if (!map->nodes.empty()) {
                s.selectedMapNodeIndex = std::min(s.selectedMapNodeIndex,
                                                  (int)map->nodes.size() - 1);
            } else {
                s.selectedMapNodeIndex = -1;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("上へ") && s.selectedMapNodeIndex > 0) {
            std::swap(map->nodes[s.selectedMapNodeIndex],
                      map->nodes[s.selectedMapNodeIndex - 1]);
            s.selectedMapNodeIndex -= 1;
            map->dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("下へ") &&
            s.selectedMapNodeIndex + 1 < (int)map->nodes.size()) {
            std::swap(map->nodes[s.selectedMapNodeIndex],
                      map->nodes[s.selectedMapNodeIndex + 1]);
            s.selectedMapNodeIndex += 1;
            map->dirty = true;
        }
    }
}

}  // namespace

namespace UI {

void DrawProjectPanel(App::EditorAppState& s) {
    ImGui::Begin("左: レベル / マップノード / ノードひな形",
                 nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse);

    ImGui::Text("データフォルダ: %s",
                s.projectLoaded ? s.project.dataRoot.generic_string().c_str()
                                : "未選択");
    if (ImGui::Button("データフォルダを開く")) {
        std::string picked;
        if (Platform::OpenFolderDialog(picked, L"データフォルダを選択")) {
            s.pendingLoadPath = picked;
            if (s.projectLoaded && s.project.HasDirty()) {
                s.promptSaveBeforeLoad = true;
            } else {
                s.promptSaveBeforeLoad = false;
                App::EditorApp app;
                if (!app.LoadDataFolder(s, s.pendingLoadPath)) {
                    App::Log(s, "プロジェクトを開けませんでした。");
                    s.pendingLoadPath.clear();
                }
            }
        }
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!s.projectLoaded);
    if (ImGui::Button("Save All")) {
        App::EditorApp app;
        if (app.SaveAll(s)) {
            App::Log(s, "保存しました。");
        } else {
            App::Log(s, "保存に失敗しました。");
        }
    }
    ImGui::EndDisabled();

    if (s.projectLoaded) {
        ImGui::Text(s.project.HasDirty() ? "* 保存未了" : "保存済み");
        ImGui::Text("levels.csv: %s", s.project.levelsPath.generic_string().c_str());
        ImGui::Text("nodes.csv: %s", s.project.nodesPath.generic_string().c_str());
    }

    if (!s.projectLoaded) {
        ImGui::TextUnformatted("編集を始めるにはデータフォルダを開いてください。");
    }
    ImGui::BeginDisabled(!s.projectLoaded);
    ImGui::PushID("levels");
    DrawLevelSection(s);
    ImGui::PopID();
    ImGui::PushID("map_nodes");
    DrawMapNodeSection(s);
    ImGui::PopID();
    ImGui::PushID("presets");
    DrawPresetSection(s);
    ImGui::PopID();
    ImGui::EndDisabled();

    if (s.promptSaveBeforeLoad) {
        ImGui::OpenPopup("保存確認");
    }

    if (ImGui::BeginPopupModal("保存確認", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("未保存の変更があります。");
        ImGui::TextUnformatted("保存してから続行しますか？");
        if (ImGui::Button("保存して開く")) {
            if (AssetPackCore::PuzzleProjectStore::SaveAll(s.project, s.issues)) {
                App::Log(s, "保存しました。");
                App::EditorApp app;
                if (app.LoadDataFolder(s, s.pendingLoadPath)) {
                    s.promptSaveBeforeLoad = false;
                    s.pendingLoadPath.clear();
                } else {
                    App::Log(s, "再読み込みできませんでした。");
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("破棄して開く")) {
            if (!s.pendingLoadPath.empty()) {
                App::EditorApp app;
                if (app.LoadDataFolder(s, s.pendingLoadPath)) {
                    s.promptSaveBeforeLoad = false;
                    s.pendingLoadPath.clear();
                    ImGui::CloseCurrentPopup();
                } else {
                    App::Log(s, "再読み込みできませんでした。");
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("キャンセル")) {
            s.promptSaveBeforeLoad = false;
            s.pendingLoadPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

}  // namespace UI
