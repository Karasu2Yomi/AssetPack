#pragma once

#include <string_view>

namespace UI {

inline constexpr const char* kProjectWindowTitle = "左: レベル / マップノード / ノードひな形";
inline constexpr const char* kViewportWindowTitle = "中央: マップ編集###中央: Puzzle Canvas";
inline constexpr const char* kInspectorWindowTitle = "右: プロパティ";
inline constexpr const char* kBottomWindowTitle = "下: 問題 / ログ";

// Only the labels are localized; CSV values remain compatible with the game.
inline constexpr const char* kNodeTypeValues[] = {"root", "follow", "end", "dead"};
inline constexpr const char* kNodeTypeLabels[] = {"始点", "中継点", "終点", "障害物"};

inline const char* NodeTypeLabel(std::string_view value) {
    for (int i = 0; i < 4; ++i) {
        if (value == kNodeTypeValues[i]) return kNodeTypeLabels[i];
    }
    return "未設定";
}

inline const char* FieldLabel(std::string_view field) {
    struct FieldName { std::string_view key; const char* label; };
    static constexpr FieldName names[] = {
        {"header", "見出し行"},
        {"level_id", "レベル識別子"},
        {"level_name", "レベル名"},
        {"map_path", "マップの保存先"},
        {"next_level_id", "次のレベルの識別子"},
        {"total_length", "血管の総延長"},
        {"minimum_slack_ratio", "最小余裕率"},
        {"background_color", "背景色"},
        {"vessel_color", "血管色"},
        {"base_width", "基本幅"},
        {"tip_width", "先端幅"},
        {"width_variation", "幅の変動率"},
        {"preset_id", "ひな形識別子"},
        {"node_type", "ノードの種類"},
        {"texture_path", "画像のパス"},
        {"width_tiles", "幅（タイル数）"},
        {"height_tiles", "高さ（タイル数）"},
        {"display_name", "表示名"},
        {"max_incoming", "最大入力接続数"},
        {"max_outgoing", "最大出力接続数"},
        {"max_outgoing_length", "出力血管の最大長"},
        {"instance_id", "ノード識別子"},
        {"source_preset_id", "参照ひな形識別子"},
        {"tile_x", "横位置（タイル）"},
        {"tile_y", "縦位置（タイル）"},
    };
    for (const auto& name : names) {
        if (name.key == field) return name.label;
    }
    return "項目";
}

}  // namespace UI
