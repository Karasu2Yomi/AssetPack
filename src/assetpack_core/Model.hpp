#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace AssetPackCore {

enum class IssueSeverity { Error, Warning };

struct ProjectIssue {
    std::string file;
    int line = -1;
    std::string field;
    IssueSeverity severity = IssueSeverity::Error;
    std::string message;
};

struct OverrideField {
    bool overridden = false;
    std::string value;
};

struct LevelRow {
    std::size_t line = 0;
    std::string level_id;
    std::string level_name;
    std::string map_path;  // resource-root-relative, inside the selected data folder
    std::string next_level_id;
    std::string total_length;
    std::string minimum_slack_ratio;
    std::string background_color;
    std::string vessel_color;
    std::string base_width;
    std::string tip_width;
    std::string width_variation;
};

struct NodePresetRow {
    std::size_t line = 0;
    std::string preset_id;
    std::string node_type;
    std::string texture_path;
    std::string width_tiles;
    std::string height_tiles;
    std::string display_name;
    std::string max_incoming;
    std::string max_outgoing;
    std::string max_outgoing_length;
};

struct MapNodeRow {
    std::size_t line = 0;
    std::string instance_id;
    std::string source_preset_id;

    OverrideField node_type;
    OverrideField texture_path;
    OverrideField width_tiles;
    OverrideField height_tiles;
    OverrideField display_name;
    OverrideField tile_x;
    OverrideField tile_y;
    OverrideField max_incoming;
    OverrideField max_outgoing;
    OverrideField max_outgoing_length;
};

struct ResolvedNodeView {
    std::string instance_id;
    std::string source_preset_id;
    std::string node_type;
    std::string texture_path;
    int width_tiles = 0;
    int height_tiles = 0;
    std::string display_name;
    bool has_placement = false;
    int tile_x = 0;
    int tile_y = 0;
    unsigned max_incoming = 0;
    unsigned max_outgoing = 0;
    float max_outgoing_length = 0.0f;
};

enum class MapDocumentState { NewDraft, MissingFileDraft, Persisted };

struct MapDocument {
    std::string map_path;
    std::vector<MapNodeRow> nodes;
    MapDocumentState state = MapDocumentState::NewDraft;
    bool dirty = true;
    bool deleted = false;
};

struct PuzzleProject {
    std::filesystem::path dataRoot;
    std::filesystem::path resourceRoot;

    std::vector<LevelRow> levels;
    std::vector<NodePresetRow> presets;
    std::unordered_map<std::string, MapDocument> maps;

    std::vector<std::string> mapLoadOrder;

    std::filesystem::path levelsPath;
    std::filesystem::path nodesPath;

    bool levelsDirty = false;
    bool presetsDirty = false;

    [[nodiscard]] bool HasDirty() const {
        if (levelsDirty || presetsDirty) {
            return true;
        }
        for (const auto& [key, map] : maps) {
            if (!map.deleted &&
                (map.dirty || map.state != MapDocumentState::Persisted)) {
                return true;
            }
        }
        return false;
    }
};

} // namespace AssetPackCore
