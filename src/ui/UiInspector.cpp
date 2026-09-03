#include "UiInspector.hpp"
#include "UiLabels.hpp"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <filesystem>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <string>

#include "app/Logger.hpp"
#include "assetpack_core/PuzzleResolver.hpp"
#include "platform/WinFileDialog.hpp"

namespace {

using namespace App;
using namespace AssetPackCore;

static LevelRow* CurrentLevel(App::EditorAppState& s) {
    if (s.selectedLevelIndex < 0 || s.selectedLevelIndex >= (int)s.project.levels.size()) {
        return nullptr;
    }
    return &s.project.levels[s.selectedLevelIndex];
}

static NodePresetRow* CurrentPreset(App::EditorAppState& s) {
    if (s.selectedPresetIndex < 0 || s.selectedPresetIndex >= (int)s.project.presets.size()) {
        return nullptr;
    }
    return &s.project.presets[s.selectedPresetIndex];
}

static MapNodeRow* CurrentMapNode(EditorAppState& s, int selected) {
    auto* map = s.SelectedMap();
    if (!map) return nullptr;
    if (selected < 0 || selected >= (int)map->nodes.size()) return nullptr;
    return &map->nodes[selected];
}

static bool IsValidId(const std::string& text) {
    if (text.empty() || text.size() > 64) return false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        const bool lower = ch >= 'a' && ch <= 'z';
        const bool digit = ch >= '0' && ch <= '9';
        const bool underscore = ch == '_';
        if (i == 0) {
            if (!(lower || underscore)) return false;
        } else {
            if (!(lower || digit || underscore)) return false;
        }
        if (underscore && i + 1 < text.size() && text[i + 1] == '_') return false;
    }
    return text.back() != '_';
}

static std::string NormalizeRelativePath(std::string path) {
    for (char& ch : path) {
        if (ch == '\\') ch = '/';
    }
    while (!path.empty() && (path.front() == '/' || path.front() == '\\')) {
        path.erase(path.begin());
    }
    while (!path.empty() && (path.back() == '/' || path.back() == '\\')) {
        path.pop_back();
    }
    return path;
}

static bool IsInsideResourceRoot(const std::filesystem::path& root,
                                const std::filesystem::path& candidate,
                                std::string* reason) {
    std::error_code ec{};
    const auto rootAbs = std::filesystem::weakly_canonical(root, ec);
    if (ec) {
        if (reason) *reason = "リソースフォルダーのパスを確認できませんでした。";
        return false;
    }

    const auto candAbs = candidate.is_absolute()
                             ? std::filesystem::weakly_canonical(candidate, ec)
                             : std::filesystem::weakly_canonical(root / candidate, ec);
    if (ec) {
        if (reason) *reason = "選択したファイルのパスを確認できませんでした。";
        return false;
    }

    auto it = candAbs.begin();
    auto jt = rootAbs.begin();
    while (jt != rootAbs.end()) {
        if (it == candAbs.end() || *it != *jt) {
            if (reason) *reason = "選択したファイルがリソースフォルダーの外にあります。";
            return false;
        }
        ++it;
        ++jt;
    }
    return true;
}

static bool RenameLevelId(EditorAppState& s, int levelIndex,
                         const std::string& from, const std::string& to) {
    if (from == to) return true;
    if (!IsValidId(to)) {
        App::Log(s, "エラー: レベル識別子は64文字以内の半角英小文字・数字・アンダースコアにしてください。先頭に数字、末尾や連続したアンダースコアは使えません。");
        return false;
    }
    auto it = std::find_if(s.project.levels.begin(), s.project.levels.end(),
                           [&](const LevelRow& l) {
                               return l.level_id == to && (&l != &s.project.levels[levelIndex]);
                           });
    if (it != s.project.levels.end()) {
        App::Log(s, "エラー: レベル識別子が重複しています。");
        return false;
    }
    for (auto& l : s.project.levels) {
        if (l.next_level_id == from) {
            l.next_level_id = to;
        }
    }
    s.project.levels[levelIndex].level_id = to;
    s.project.levelsDirty = true;
    return true;
}

static bool RenamePresetId(App::EditorAppState& s, int presetIndex,
                          const std::string& from, const std::string& to) {
    if (from == to) return true;
    if (!IsValidId(to)) {
        App::Log(s, "エラー: ひな形識別子は64文字以内の半角英小文字・数字・アンダースコアにしてください。先頭に数字、末尾や連続したアンダースコアは使えません。");
        return false;
    }
    auto it = std::find_if(s.project.presets.begin(), s.project.presets.end(),
                           [&](const NodePresetRow& p) {
                               return p.preset_id == to &&
                                      (&p != &s.project.presets[presetIndex]);
                           });
    if (it != s.project.presets.end()) {
        App::Log(s, "エラー: ひな形識別子が重複しています。");
        return false;
    }
    for (auto& mapPair : s.project.maps) {
        for (auto& node : mapPair.second.nodes) {
            if (node.source_preset_id == from) {
                node.source_preset_id = to;
                mapPair.second.dirty = true;
            }
        }
    }
    s.project.presets[presetIndex].preset_id = to;
    s.project.presetsDirty = true;
    return true;
}

static bool InstanceIdDuplicated(const MapDocument& map, const std::string& id,
                                int exceptIndex) {
    for (int i = 0; i < (int)map.nodes.size(); ++i) {
        if (i == exceptIndex) continue;
        if (map.nodes[i].instance_id == id) {
            return true;
        }
    }
    return false;
}

static void DrawFieldLabel(const char* id) {
    ImGui::TextWrapped("%s", UI::FieldLabel(id));
}

static bool DrawTextField(const char* id, std::string* value,
                          ImGuiInputTextFlags flags = ImGuiInputTextFlags_None) {
    DrawFieldLabel(id);
    ImGui::SetNextItemWidth(-FLT_MIN);
    return ImGui::InputText((std::string("##") + id).c_str(), value, flags);
}

static bool DrawNodeTypeCombo(const char* id, std::string& value,
                              bool allowUnset = false) {
    bool changed = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo((std::string("##") + id).c_str(), UI::NodeTypeLabel(value))) {
        if (allowUnset && ImGui::Selectable("未設定##empty", value.empty())) {
            value.clear();
            changed = true;
        }
        for (int i = 0; i < 4; ++i) {
            if (ImGui::Selectable(UI::kNodeTypeLabels[i], value == UI::kNodeTypeValues[i])) {
                value = UI::kNodeTypeValues[i];
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

static bool DrawOverrideToggle(const char* id, OverrideField& field) {
    const auto& style = ImGui::GetStyle();
    const float rowWidth = ImGui::CalcTextSize(UI::FieldLabel(id)).x + style.ItemSpacing.x +
                           ImGui::GetFrameHeight() + style.ItemInnerSpacing.x +
                           ImGui::CalcTextSize("上書き").x;
    const bool sameLine = rowWidth <= ImGui::GetContentRegionAvail().x;
    DrawFieldLabel(id);
    if (sameLine) ImGui::SameLine();
    return ImGui::Checkbox("上書き##override", &field.overridden);
}

static bool DrawOverrideTextField(const char* id, OverrideField& field,
                                 const std::string& inherited) {
    ImGui::PushID(id);
    bool changed = DrawOverrideToggle(id, field);
    if (field.overridden) {
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText("##value", &field.value)) {
            changed = true;
        }
    } else {
        ImGui::TextWrapped("%s", inherited.empty() ? "（継承）" : inherited.c_str());
    }
    ImGui::PopID();
    return changed;
}

static bool DrawOverrideNodeTypeField(OverrideField& field,
                                      const std::string& inherited) {
    ImGui::PushID("node_type");
    bool changed = DrawOverrideToggle("node_type", field);
    if (field.overridden) {
        changed = DrawNodeTypeCombo("value", field.value, true) || changed;
    } else {
        ImGui::Text("%s（継承）", UI::NodeTypeLabel(inherited));
    }
    ImGui::PopID();
    return changed;
}

static ResolvedNodeView ResolveCurrentNode(const EditorAppState& s, const MapNodeRow& node) {
    const NodePresetRow* preset = nullptr;
    for (const auto& p : s.project.presets) {
        if (p.preset_id == node.source_preset_id) {
            preset = &p;
            break;
        }
    }
    return PuzzleResolver::Resolve(preset ? *preset : NodePresetRow{}, node);
}

static void DrawLevelInspector(App::EditorAppState& s) {
    auto* l = CurrentLevel(s);
    if (!l) return;

    ImGui::SeparatorText("レベル設定");
    bool dirty = false;

    std::string beforeId = l->level_id;
    if (DrawTextField("level_id", &l->level_id)) {
        if (!RenameLevelId(s, s.selectedLevelIndex, beforeId, l->level_id)) {
            l->level_id = beforeId;
        }
        dirty = true;
    }

    if (DrawTextField("level_name", &l->level_name)) {
        dirty = true;
    }
    DrawTextField("map_path", &l->map_path, ImGuiInputTextFlags_ReadOnly);
    if (DrawTextField("next_level_id", &l->next_level_id)) {
        dirty = true;
    }
    if (DrawTextField("total_length", &l->total_length)) {
        dirty = true;
    }
    if (DrawTextField("minimum_slack_ratio", &l->minimum_slack_ratio)) {
        dirty = true;
    }
    if (DrawTextField("background_color", &l->background_color)) {
        dirty = true;
    }
    if (DrawTextField("vessel_color", &l->vessel_color)) {
        dirty = true;
    }
    if (DrawTextField("base_width", &l->base_width)) {
        dirty = true;
    }
    if (DrawTextField("tip_width", &l->tip_width)) {
        dirty = true;
    }
    if (DrawTextField("width_variation", &l->width_variation)) {
        dirty = true;
    }
    if (dirty) {
        s.project.levelsDirty = true;
    }
}

static void DrawPresetInspector(App::EditorAppState& s) {
    auto* p = CurrentPreset(s);
    if (!p) return;

    ImGui::SeparatorText("ノードひな形設定");
    bool dirty = false;

    std::string beforeId = p->preset_id;
    if (DrawTextField("preset_id", &p->preset_id)) {
        if (!RenamePresetId(s, s.selectedPresetIndex, beforeId, p->preset_id)) {
            p->preset_id = beforeId;
        }
        dirty = true;
    }

    DrawFieldLabel("node_type");
    if (DrawNodeTypeCombo("node_type", p->node_type)) {
        dirty = true;
    }
    if (DrawTextField("texture_path", &p->texture_path)) {
        dirty = true;
    }
    if (DrawTextField("width_tiles", &p->width_tiles)) {
        dirty = true;
    }
    if (DrawTextField("height_tiles", &p->height_tiles)) {
        dirty = true;
    }
    if (DrawTextField("display_name", &p->display_name)) {
        dirty = true;
    }
    if (DrawTextField("max_incoming", &p->max_incoming)) {
        dirty = true;
    }
    if (DrawTextField("max_outgoing", &p->max_outgoing)) {
        dirty = true;
    }
    if (DrawTextField("max_outgoing_length", &p->max_outgoing_length)) {
        dirty = true;
    }
    if (ImGui::Button("画像を選択##choose_texture")) {
        std::string picked;
        if (Platform::OpenFileDialog(picked, L"画像を選択", L"画像 (*.png;*.jpg;*.jpeg)",
                                    L"*.png;*.jpg;*.jpeg")) {
            std::filesystem::path chosen{picked};
            std::string reason;
            if (IsInsideResourceRoot(s.project.resourceRoot, chosen, &reason)) {
                std::string rel;
                if (chosen.is_absolute()) {
                    rel = std::filesystem::relative(chosen, s.project.resourceRoot).generic_string();
                } else {
                    rel = picked;
                }
                p->texture_path = NormalizeRelativePath(rel);
                dirty = true;
            } else {
                std::string msg = "画像を選択できません: " + reason;
                App::Log(s, msg);
            }
        }
    }

    if (dirty) {
        s.project.presetsDirty = true;
    }
}

static void DrawMapNodeInspector(App::EditorAppState& s) {
    auto* map = s.SelectedMap();
    auto* node = CurrentMapNode(s, s.selectedMapNodeIndex);
    if (!node || !map) return;

    ImGui::SeparatorText("マップノード設定");

    const auto resolved = ResolveCurrentNode(s, *node);
    bool dirty = false;

    std::string beforeInstance = node->instance_id;
    if (DrawTextField("instance_id", &node->instance_id)) {
        if (!IsValidId(node->instance_id) ||
            InstanceIdDuplicated(*map, node->instance_id, s.selectedMapNodeIndex)) {
            App::Log(s, "エラー: ノード識別子の形式が正しくないか、重複しています。元に戻しました。");
            node->instance_id = beforeInstance;
        } else {
            dirty = true;
        }
    }

    if (DrawTextField("source_preset_id", &node->source_preset_id)) {
        bool found = false;
        for (const auto& preset : s.project.presets) {
            if (preset.preset_id == node->source_preset_id) {
                found = true;
                break;
            }
        }
        if (!node->source_preset_id.empty() && !found) {
            App::Log(s, "エラー: 指定した識別子のひな形が見つからないため、保存できません。");
        }
        dirty = true;
    }
    if (DrawOverrideNodeTypeField(node->node_type, resolved.node_type)) {
        dirty = true;
    }
    if (DrawOverrideTextField("texture_path", node->texture_path,
                             resolved.texture_path)) {
        dirty = true;
    }
    if (DrawOverrideTextField("width_tiles", node->width_tiles,
                             std::to_string(resolved.width_tiles))) {
        dirty = true;
    }
    if (DrawOverrideTextField("height_tiles", node->height_tiles,
                             std::to_string(resolved.height_tiles))) {
        dirty = true;
    }
    if (DrawOverrideTextField("display_name", node->display_name,
                             resolved.display_name)) {
        dirty = true;
    }
    if (DrawOverrideTextField("max_incoming", node->max_incoming,
                             std::to_string(resolved.max_incoming))) {
        dirty = true;
    }
    if (DrawOverrideTextField("max_outgoing", node->max_outgoing,
                             std::to_string(resolved.max_outgoing))) {
        dirty = true;
    }
    if (DrawOverrideTextField("max_outgoing_length",
                             node->max_outgoing_length,
                             std::to_string(resolved.max_outgoing_length))) {
        dirty = true;
    }

    bool tileXChanged = ImGui::Checkbox("横位置を上書き##override_tile_x", &node->tile_x.overridden);
    if (node->tile_x.overridden) {
        if (DrawTextField("tile_x", &node->tile_x.value)) {
            tileXChanged = true;
        }
    } else {
        ImGui::TextUnformatted("横位置: （継承）");
    }
    bool tileYChanged = ImGui::Checkbox("縦位置を上書き##override_tile_y", &node->tile_y.overridden);
    if (node->tile_y.overridden) {
        if (DrawTextField("tile_y", &node->tile_y.value)) {
            tileYChanged = true;
        }
    } else {
        ImGui::TextUnformatted("縦位置: （継承）");
    }
    dirty = dirty || tileXChanged || tileYChanged;

    if (ImGui::Button("配置を解除##clear_placement")) {
        node->tile_x.overridden = true;
        node->tile_y.overridden = true;
        node->tile_x.value.clear();
        node->tile_y.value.clear();
        dirty = true;
        App::Log(s, "ノードを未配置にしました。");
    }

    if (dirty) {
        map->dirty = true;
    }
}

}  // namespace

namespace UI {

void DrawInspectorPanel(App::EditorAppState& s) {
    ImGui::Begin(kInspectorWindowTitle,
                 nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse);

    switch (s.selectedTarget) {
        case SelectTarget::Level:
            DrawLevelInspector(s);
            break;
        case SelectTarget::Preset:
            DrawPresetInspector(s);
            break;
        case SelectTarget::MapNode:
            DrawMapNodeInspector(s);
            break;
        default:
            ImGui::TextUnformatted("左側で対象を選択してください。");
            break;
    }

    ImGui::End();
}

}  // namespace UI
