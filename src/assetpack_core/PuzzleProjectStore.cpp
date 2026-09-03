#include "PuzzleProjectStore.hpp"

#include "CsvCodec.hpp"

#include <array>
#include <charconv>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cwctype>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

namespace {

using namespace AssetPackCore;

constexpr std::array<std::string_view, 11> kLevelsHeader{
    "level_id", "level_name", "map_path", "next_level_id", "total_length",
    "minimum_slack_ratio", "background_color", "vessel_color", "base_width",
    "tip_width", "width_variation",
};

constexpr std::array<std::string_view, 9> kNodePresetsHeader{
    "preset_id", "node_type", "texture_path", "width_tiles", "height_tiles",
    "display_name", "max_incoming", "max_outgoing",
    "max_outgoing_length",
};

constexpr std::array<std::string_view, 12> kMapHeader{
    "instance_id", "source_preset_id", "node_type", "texture_path", "width_tiles",
    "height_tiles", "display_name", "tile_x", "tile_y", "max_incoming",
    "max_outgoing", "max_outgoing_length",
};

void AddIssue(std::vector<ProjectIssue>& issues, const std::string& file, int line,
              const std::string& field, IssueSeverity severity,
              const std::string& message) {
    issues.push_back(ProjectIssue{file, line, field, severity, message});
}

bool AnyError(const std::vector<ProjectIssue>& issues) {
    for (const auto& issue : issues) {
        if (issue.severity == IssueSeverity::Error) {
            return true;
        }
    }
    return false;
}

std::string NormalizePathForCsv(std::string text) {
    for (char& ch : text) {
        if (ch == '\\') ch = '/';
    }
    return text;
}

bool HasPathViolation(const std::filesystem::path& p,
                      bool requireRegularRelative) {
    auto r = p;
    if (!r.is_relative() && requireRegularRelative) {
        return true;
    }
    if (r.empty()) {
        return true;
    }
    std::string raw = p.generic_string();
    if (raw.empty() || raw.find('\0') != std::string::npos ||
        raw.find('\r') != std::string::npos ||
        raw.find('\n') != std::string::npos || raw.find(':') != std::string::npos) {
        return true;
    }
    return false;
}

bool IsInside(const std::filesystem::path& target,
              const std::filesystem::path& base) {
    std::error_code ec{};
    const auto targetAbs = std::filesystem::weakly_canonical(target, ec);
    if (ec) {
        return false;
    }
    const auto baseAbs = std::filesystem::weakly_canonical(base, ec);
    if (ec) {
        return false;
    }
    auto ti = targetAbs.begin();
    auto bi = baseAbs.begin();
    while (bi != baseAbs.end()) {
        if (ti == targetAbs.end() || *ti != *bi) {
            return false;
        }
        ++ti;
        ++bi;
    }
    return true;
}

bool PathEntryExists(const std::filesystem::path& path, std::error_code& ec) {
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory) {
        ec.clear();
        return false;
    }
    return !ec && std::filesystem::exists(status);
}

bool EqualHeader(const std::vector<std::string>& header,
                const std::vector<std::string_view>& expected) {
    if (header.size() != expected.size()) return false;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (header[i] != expected[i]) return false;
    }
    return true;
}

template <std::size_t N>
bool EqualHeader(const std::vector<std::string>& header,
                const std::array<std::string_view, N>& expected,
                const std::string& file, int line,
                std::vector<ProjectIssue>& issues) {
    if (!EqualHeader(header, std::vector<std::string_view>{expected.begin(),
                                                           expected.end()})) {
        AddIssue(issues, file, line, "header", IssueSeverity::Error,
                 "header must be fixed field order");
        return false;
    }
    return true;
}

bool IsValidIdentifier(const std::string& text) {
    if (text.empty() || text.size() > 64 || text.front() < 'a' || text.front() > 'z' ||
        text.back() == '_') {
        return false;
    }
    bool prevUnderscore = false;
    for (const unsigned char ch : text) {
        const bool lower = ch >= 'a' && ch <= 'z';
        const bool digit = ch >= '0' && ch <= '9';
        const bool underscore = ch == '_';
        if ((!lower && !digit && !underscore) || (underscore && prevUnderscore)) {
            return false;
        }
        prevUnderscore = underscore;
    }
    return true;
}

bool IsValidNodeType(const std::string& text) {
    return text == "root" || text == "follow" || text == "end" || text == "dead";
}

bool ParseUnsigned(const std::string& text, std::uint32_t& out) {
    auto begin = text.data();
    auto end = begin + text.size();
    auto rc = std::from_chars(begin, end, out, 10);
    return rc.ec == std::errc{} && rc.ptr == end;
}

bool ParseUnsigned(const std::string& text, int& out) {
    if (text.empty()) return false;
    std::uint32_t value = 0;
    if (!ParseUnsigned(text, value)) return false;
    out = static_cast<int>(value);
    return true;
}

bool ParseFloat(const std::string& text, float& out, bool allowEmpty) {
    if (text.empty()) return allowEmpty;
    const char* begin = text.c_str();
    char* end = nullptr;
    const float value = std::strtof(begin, &end);
    if (end == begin || *end != '\0' || !std::isfinite(value) || value < 0.0f) {
        return false;
    }
    out = value;
    return true;
}

bool ValidateColor(const std::string& text) {
    if (text.empty()) return true;
    if (text.size() != 7 && text.size() != 9) return false;
    if (text.front() != '#') return false;
    for (std::size_t i = 1; i < text.size(); ++i) {
        const char c = text[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}

bool ValidateRelativePathOnly(const std::string& text, const std::string& file,
                             int line, const std::string& field,
                             std::vector<ProjectIssue>& issues) {
    if (text.empty()) {
        AddIssue(issues, file, line, field, IssueSeverity::Error,
                 "path is required");
        return false;
    }
    if (HasPathViolation(std::filesystem::path{text}, true)) {
        AddIssue(issues, file, line, field, IssueSeverity::Error,
                 "path must be non-empty, single-line, and relative");
        return false;
    }
    const auto rel = std::filesystem::path{NormalizePathForCsv(text)};
    for (const auto& part : rel) {
        if (part.empty() || part == "." || part == "..") {
            AddIssue(issues, file, line, field, IssueSeverity::Error,
                     "path must remain within resource root");
            return false;
        }
    }
    return true;
}

bool ValidateAndParseLevel(const CsvRecord& row, const std::filesystem::path& dataRoot,
                          const std::filesystem::path& resourceRoot,
                          std::vector<LevelRow>& out,
                          std::vector<std::string>& mapOrder,
                          std::unordered_set<std::string>& levelIds,
                          std::vector<ProjectIssue>& issues) {
    if (row.fields.size() != kLevelsHeader.size()) {
        AddIssue(issues, "levels.csv", static_cast<int>(row.lineNumber), {},
                 IssueSeverity::Error, "record field count mismatch");
        return false;
    }
    LevelRow level;
    level.line = row.lineNumber;
    level.level_id = row.fields[0];
    level.level_name = row.fields[1];
    level.map_path = NormalizePathForCsv(row.fields[2]);
    level.next_level_id = row.fields[3];
    level.total_length = row.fields[4];
    level.minimum_slack_ratio = row.fields[5];
    level.background_color = row.fields[6];
    level.vessel_color = row.fields[7];
    level.base_width = row.fields[8];
    level.tip_width = row.fields[9];
    level.width_variation = row.fields[10];

    if (!IsValidIdentifier(level.level_id)) {
        AddIssue(issues, "levels.csv", static_cast<int>(level.line), "level_id",
                 IssueSeverity::Error, "ID must be lower_snake_case");
    } else if (!levelIds.insert(level.level_id).second) {
        AddIssue(issues, "levels.csv", static_cast<int>(level.line), "level_id",
                 IssueSeverity::Error, "duplicate level_id");
    }
    if (level.level_name.empty()) {
        AddIssue(issues, "levels.csv", static_cast<int>(level.line), "level_name",
                 IssueSeverity::Error, "level_name is required");
    }
    if (ValidateRelativePathOnly(level.map_path, "levels.csv",
                                static_cast<int>(level.line), "map_path", issues)) {
        const auto mapRel = std::filesystem::path{level.map_path};
        const auto mapAbs = resourceRoot / mapRel;
        if (!IsInside(mapAbs, dataRoot)) {
            AddIssue(issues, "levels.csv", static_cast<int>(level.line), "map_path",
                     IssueSeverity::Error,
                     "map_path must resolve inside selected data folder");
        }
        if (mapOrder.end() == std::find(mapOrder.begin(), mapOrder.end(),
                                        level.map_path)) {
            mapOrder.push_back(level.map_path);
        }
    }
    if (!level.next_level_id.empty() && !IsValidIdentifier(level.next_level_id)) {
        AddIssue(issues, "levels.csv", static_cast<int>(level.line), "next_level_id",
                 IssueSeverity::Warning,
                 "next_level_id is not in lower_snake_case");
    }
    float parsed = 0.0f;
    if (!ParseFloat(level.total_length, parsed, true)) {
        AddIssue(issues, "levels.csv", static_cast<int>(level.line), "total_length",
                 IssueSeverity::Error, "must be non-negative decimal");
    }
    if (!ParseFloat(level.minimum_slack_ratio, parsed, true)) {
        AddIssue(issues, "levels.csv", static_cast<int>(level.line),
                 "minimum_slack_ratio", IssueSeverity::Error,
                 "must be non-negative decimal");
    }
    if (!ParseFloat(level.base_width, parsed, true)) {
        AddIssue(issues, "levels.csv", static_cast<int>(level.line), "base_width",
                 IssueSeverity::Error, "must be non-negative decimal");
    }
    if (!ParseFloat(level.tip_width, parsed, true)) {
        AddIssue(issues, "levels.csv", static_cast<int>(level.line), "tip_width",
                 IssueSeverity::Error, "must be non-negative decimal");
    }
    if (!ParseFloat(level.width_variation, parsed, true)) {
        AddIssue(issues, "levels.csv", static_cast<int>(level.line), "width_variation",
                 IssueSeverity::Error, "must be non-negative decimal");
    }
    if (!ValidateColor(level.background_color)) {
        AddIssue(issues, "levels.csv", static_cast<int>(level.line),
                 "background_color", IssueSeverity::Error,
                 "expected #RRGGBB or #RRGGBBAA, or blank");
    }
    if (!ValidateColor(level.vessel_color)) {
        AddIssue(issues, "levels.csv", static_cast<int>(level.line), "vessel_color",
                 IssueSeverity::Error, "expected #RRGGBB or #RRGGBBAA, or blank");
    }

    out.push_back(std::move(level));
    return true;
}

bool ValidateAndParsePreset(const CsvRecord& row,
                           std::vector<NodePresetRow>& out,
                           std::unordered_set<std::string>& presetIds,
                           const std::filesystem::path& resourceRoot,
                           std::vector<ProjectIssue>& issues) {
    if (row.fields.size() != kNodePresetsHeader.size()) {
        AddIssue(issues, "nodes.csv", static_cast<int>(row.lineNumber), {},
                 IssueSeverity::Error, "record field count mismatch");
        return false;
    }

    NodePresetRow preset;
    preset.line = row.lineNumber;
    preset.preset_id = row.fields[0];
    preset.node_type = row.fields[1];
    preset.texture_path = NormalizePathForCsv(row.fields[2]);
    preset.width_tiles = row.fields[3];
    preset.height_tiles = row.fields[4];
    preset.display_name = row.fields[5];
    preset.max_incoming = row.fields[6];
    preset.max_outgoing = row.fields[7];
    preset.max_outgoing_length = row.fields[8];

    if (!IsValidIdentifier(preset.preset_id)) {
        AddIssue(issues, "nodes.csv", static_cast<int>(preset.line), "preset_id",
                 IssueSeverity::Error, "ID must be lower_snake_case");
    } else if (!presetIds.insert(preset.preset_id).second) {
        AddIssue(issues, "nodes.csv", static_cast<int>(preset.line), "preset_id",
                 IssueSeverity::Error, "duplicate preset_id");
    }
    if (!IsValidNodeType(preset.node_type)) {
        AddIssue(issues, "nodes.csv", static_cast<int>(preset.line), "node_type",
                 IssueSeverity::Error, "expected root/follow/end/dead");
    }
    if (!preset.texture_path.empty()) {
        ValidateRelativePathOnly(preset.texture_path, "nodes.csv",
                                static_cast<int>(preset.line), "texture_path",
                                issues);
        const auto tex = resourceRoot / std::filesystem::path{preset.texture_path};
        if (!std::filesystem::is_regular_file(tex)) {
            AddIssue(issues, "nodes.csv", static_cast<int>(preset.line),
                     "texture_path", IssueSeverity::Warning,
                     "referenced file does not exist");
        }
    }
    std::uint32_t u32 = 0;
    float f = 0.0f;
    if (!preset.width_tiles.empty() && !ParseUnsigned(preset.width_tiles, u32)) {
        AddIssue(issues, "nodes.csv", static_cast<int>(preset.line), "width_tiles",
                 IssueSeverity::Error, "must be non-negative integer");
    }
    if (!preset.height_tiles.empty() && !ParseUnsigned(preset.height_tiles, u32)) {
        AddIssue(issues, "nodes.csv", static_cast<int>(preset.line), "height_tiles",
                 IssueSeverity::Error, "must be non-negative integer");
    }
    if (!preset.max_incoming.empty() && !ParseUnsigned(preset.max_incoming, u32)) {
        AddIssue(issues, "nodes.csv", static_cast<int>(preset.line), "max_incoming",
                 IssueSeverity::Error, "must be non-negative integer");
    }
    if (!preset.max_outgoing.empty() && !ParseUnsigned(preset.max_outgoing, u32)) {
        AddIssue(issues, "nodes.csv", static_cast<int>(preset.line), "max_outgoing",
                 IssueSeverity::Error, "must be non-negative integer");
    }
    if (!ParseFloat(preset.max_outgoing_length, f, true)) {
        AddIssue(issues, "nodes.csv", static_cast<int>(preset.line),
                 "max_outgoing_length", IssueSeverity::Error,
                 "must be non-negative decimal");
    }

    out.push_back(std::move(preset));
    return true;
}

bool ValidateAndParseMapNode(const CsvRecord& row,
                            const std::unordered_set<std::string>& presetIds,
                            const std::filesystem::path& resourceRoot,
                            MapDocument& map,
                            std::vector<ProjectIssue>& issues) {
    if (row.fields.size() != kMapHeader.size()) {
        AddIssue(issues, map.map_path, static_cast<int>(row.lineNumber), {},
                 IssueSeverity::Error, "record field count mismatch");
        return false;
    }

    MapNodeRow node;
    node.line = row.lineNumber;
    node.instance_id = row.fields[0];
    node.source_preset_id = row.fields[1];

    node.node_type = { !row.fields[2].empty(), row.fields[2] };
    node.texture_path = { !row.fields[3].empty(), row.fields[3] };
    node.width_tiles = { !row.fields[4].empty(), row.fields[4] };
    node.height_tiles = { !row.fields[5].empty(), row.fields[5] };
    node.display_name = { !row.fields[6].empty(), row.fields[6] };
    node.tile_x = { !row.fields[7].empty(), row.fields[7] };
    node.tile_y = { !row.fields[8].empty(), row.fields[8] };
    node.max_incoming = { !row.fields[9].empty(), row.fields[9] };
    node.max_outgoing = { !row.fields[10].empty(), row.fields[10] };
    node.max_outgoing_length = { !row.fields[11].empty(), row.fields[11] };

    if (!IsValidIdentifier(node.instance_id)) {
        AddIssue(issues, map.map_path, static_cast<int>(node.line), "instance_id",
                 IssueSeverity::Error, "ID must be lower_snake_case");
    }
    const bool hasPreset = !node.source_preset_id.empty();
    if (!hasPreset && !node.node_type.overridden) {
        AddIssue(issues, map.map_path, static_cast<int>(node.line), "node_type",
                 IssueSeverity::Error,
                 "node_type is required when source_preset_id is empty");
    }
    if (hasPreset && !presetIds.count(node.source_preset_id)) {
        AddIssue(issues, map.map_path, static_cast<int>(node.line),
                 "source_preset_id", IssueSeverity::Error, "unknown source_preset_id");
    }
    if (node.node_type.overridden && !IsValidNodeType(node.node_type.value)) {
        AddIssue(issues, map.map_path, static_cast<int>(node.line), "node_type",
                 IssueSeverity::Error, "expected root/follow/end/dead");
    }
    if (node.texture_path.overridden && !ValidateRelativePathOnly(
                                        node.texture_path.value, map.map_path,
                                        static_cast<int>(node.line),
                                        "texture_path", issues)) {
        // issue already pushed
    }
    if (node.texture_path.overridden) {
        const auto tex = resourceRoot / node.texture_path.value;
        if (!std::filesystem::is_regular_file(tex)) {
            AddIssue(issues, map.map_path, static_cast<int>(node.line),
                     "texture_path", IssueSeverity::Warning,
                     "referenced file does not exist");
        }
    }

    std::uint32_t u32 = 0;
    int i32 = 0;
    float f = 0.0f;
    if (node.width_tiles.overridden && !ParseUnsigned(node.width_tiles.value, u32)) {
        AddIssue(issues, map.map_path, static_cast<int>(node.line), "width_tiles",
                 IssueSeverity::Error, "must be non-negative integer");
    }
    if (node.height_tiles.overridden && !ParseUnsigned(node.height_tiles.value, u32)) {
        AddIssue(issues, map.map_path, static_cast<int>(node.line), "height_tiles",
                 IssueSeverity::Error, "must be non-negative integer");
    }
    if (node.max_incoming.overridden &&
        !ParseUnsigned(node.max_incoming.value, u32)) {
        AddIssue(issues, map.map_path, static_cast<int>(node.line),
                 "max_incoming", IssueSeverity::Error,
                 "must be non-negative integer");
    }
    if (node.max_outgoing.overridden &&
        !ParseUnsigned(node.max_outgoing.value, u32)) {
        AddIssue(issues, map.map_path, static_cast<int>(node.line),
                 "max_outgoing", IssueSeverity::Error,
                 "must be non-negative integer");
    }
    if (node.max_outgoing_length.overridden &&
        !ParseFloat(node.max_outgoing_length.value, f, true)) {
        AddIssue(issues, map.map_path, static_cast<int>(node.line),
                 "max_outgoing_length", IssueSeverity::Error,
                 "must be non-negative decimal");
    }
    if (node.tile_x.overridden != node.tile_y.overridden) {
        AddIssue(issues, map.map_path, static_cast<int>(node.line), "tile_x",
                 IssueSeverity::Error, "tile_x and tile_y must be both empty or both set");
    }
    if (node.tile_x.overridden &&
        (!ParseUnsigned(node.tile_x.value, i32) || !ParseUnsigned(node.tile_y.value, i32))) {
        AddIssue(issues, map.map_path, static_cast<int>(node.line), "tile_x",
                 IssueSeverity::Error, "tile_x/tile_y must be non-negative integers");
    }
    if (node.display_name.overridden && node.display_name.value.empty()) {
        AddIssue(issues, map.map_path, static_cast<int>(node.line), "display_name",
                 IssueSeverity::Error, "display_name cannot be empty when override");
    }
    map.nodes.push_back(std::move(node));
    return true;
}

bool LoadText(const std::filesystem::path& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return !file.bad();
}

void BuildLevelDocument(const PuzzleProject& project, CsvDocument& out) {
    out.header.clear();
    out.records.clear();
    out.header.reserve(kLevelsHeader.size());
    for (const auto h : kLevelsHeader) out.header.push_back(std::string{h});
    out.records.reserve(project.levels.size());
    for (const auto& level : project.levels) {
        out.records.push_back(
            {0, {level.level_id, level.level_name, level.map_path, level.next_level_id,
                 level.total_length, level.minimum_slack_ratio,
                 level.background_color, level.vessel_color, level.base_width,
                 level.tip_width, level.width_variation}});
    }
}

void BuildPresetDocument(const PuzzleProject& project, CsvDocument& out) {
    out.header.clear();
    out.records.clear();
    out.header.reserve(kNodePresetsHeader.size());
    for (const auto h : kNodePresetsHeader) out.header.push_back(std::string{h});
    out.records.reserve(project.presets.size());
    for (const auto& preset : project.presets) {
        out.records.push_back(
            {0, {preset.preset_id, preset.node_type, preset.texture_path,
                 preset.width_tiles, preset.height_tiles, preset.display_name,
                 preset.max_incoming, preset.max_outgoing, preset.max_outgoing_length}});
    }
}

void BuildMapDocument(const MapDocument& mapDoc, CsvDocument& out) {
    out.header.clear();
    out.records.clear();
    for (const auto h : kMapHeader) out.header.push_back(std::string{h});
    out.records.reserve(mapDoc.nodes.size());
    for (const auto& node : mapDoc.nodes) {
        out.records.push_back({0,
                               {node.instance_id, node.source_preset_id,
                                node.node_type.overridden ? node.node_type.value : "",
                                node.texture_path.overridden ? node.texture_path.value : "",
                                node.width_tiles.overridden ? node.width_tiles.value : "",
                                node.height_tiles.overridden ? node.height_tiles.value : "",
                                node.display_name.overridden ? node.display_name.value : "",
                                node.tile_x.overridden ? node.tile_x.value : "",
                                node.tile_y.overridden ? node.tile_y.value : "",
                                node.max_incoming.overridden ? node.max_incoming.value : "",
                                node.max_outgoing.overridden ? node.max_outgoing.value : "",
                                node.max_outgoing_length.overridden
                                    ? node.max_outgoing_length.value
                                    : ""}});
    }
}

bool ParseAndValidateCurrentProject(const PuzzleProject& project,
                                   std::vector<ProjectIssue>& issues) {
    CsvDocument levels;
    CsvDocument presets;
    BuildLevelDocument(project, levels);
    BuildPresetDocument(project, presets);

    if (!EqualHeader(levels.header, kLevelsHeader, "levels.csv", 1, issues)) return false;
    if (!EqualHeader(presets.header, kNodePresetsHeader, "nodes.csv", 1, issues))
        return false;

    std::vector<std::string> mapOrder;
    std::vector<LevelRow> parsedLevels;
    std::unordered_set<std::string> levelIds;
    std::unordered_set<std::string> presetIds;
    std::vector<NodePresetRow> parsedPresets;

    for (const auto& rec : levels.records) {
        ValidateAndParseLevel(rec, project.dataRoot, project.resourceRoot, parsedLevels,
                             mapOrder, levelIds, issues);
    }
    for (const auto& rec : presets.records) {
        ValidateAndParsePreset(rec, parsedPresets, presetIds, project.resourceRoot,
                               issues);
    }

    for (const auto& level : project.levels) {
        const auto map = project.maps.find(level.map_path);
        if (map == project.maps.end() || map->second.deleted) {
            AddIssue(issues, "levels.csv", static_cast<int>(level.line), "map_path",
                     IssueSeverity::Error,
                     "referenced map document is missing from the project");
        }
    }

    for (const auto& mapPair : project.maps) {
        const auto& map = mapPair.second;
        if (map.deleted) continue;
        if (mapPair.first != map.map_path) {
            AddIssue(issues, mapPair.first, -1, "map_path", IssueSeverity::Error,
                     "map document path does not match its project key");
        }
        if (mapPair.first.empty()) {
            AddIssue(issues, mapPair.first, -1, "map_path", IssueSeverity::Error,
                     "map_path is required");
        } else if (ValidateRelativePathOnly(mapPair.first, mapPair.first, -1,
                                           "map_path", issues)) {
            const auto mapAbs = project.resourceRoot / mapPair.first;
            if (!IsInside(mapAbs, project.dataRoot)) {
                AddIssue(issues, mapPair.first, -1, "map_path", IssueSeverity::Error,
                         "map_path must resolve inside selected data folder");
            }
            std::error_code ec;
            const bool exists = PathEntryExists(mapAbs, ec);
            if (ec) {
                AddIssue(issues, mapPair.first, -1, "map_path", IssueSeverity::Error,
                         "cannot inspect map destination: " + ec.message());
            } else if (exists && map.state != MapDocumentState::Persisted) {
                AddIssue(issues, mapPair.first, -1, "map_path", IssueSeverity::Error,
                         "draft map destination already exists; refusing to overwrite it");
            } else if (exists && !std::filesystem::is_regular_file(mapAbs, ec)) {
                AddIssue(issues, mapPair.first, -1, "map_path", IssueSeverity::Error,
                         "map destination is not a readable regular file");
            }
        }
        CsvDocument mapDoc;
        BuildMapDocument(map, mapDoc);
        if (!EqualHeader(mapDoc.header, kMapHeader, map.map_path, 1, issues))
            continue;
        std::unordered_set<std::string> instanceIds;
        for (const auto& rec : mapDoc.records) {
            MapDocument tmp;
            tmp.map_path = mapPair.first;
            if (ValidateAndParseMapNode(rec, presetIds, project.resourceRoot, tmp, issues)) {
                if (!instanceIds.insert(tmp.nodes.back().instance_id).second) {
                    AddIssue(issues, mapPair.first, static_cast<int>(tmp.nodes.back().line),
                             "instance_id", IssueSeverity::Error,
                             "instance_id duplicated in map");
                }
            }
        }
    }
    return !AnyError(issues);
}

bool WriteTextAtomic(const std::filesystem::path& target,
                     const std::string& text) {
    const auto parent = target.parent_path();
    std::error_code ec;
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) return false;
    }
    const std::filesystem::path temp = target.string() + ".tmp";
    bool written = false;
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out << text;
        out.close();
        written = !out.fail();
    }
    if (!written) {
        // The file was opened by this save; leave pre-existing directories alone.
        std::filesystem::remove(temp, ec);
    }
    return written;
}

void RemoveTempFiles(const std::vector<std::filesystem::path>& tempFiles) {
    for (const auto& temp : tempFiles) {
        std::error_code ec;
        std::filesystem::remove(temp, ec);
    }
}

bool ReplaceWrittenFiles(const std::vector<std::filesystem::path>& targets,
                        const std::vector<std::filesystem::path>& tempFiles,
                        const std::vector<bool>& allowOverwrite,
                        std::vector<ProjectIssue>& issues) {
    struct Replacement {
        std::filesystem::path target;
        std::filesystem::path backup;
        bool existed = false;
        bool changed = false;
    };
    std::vector<Replacement> replacements;
    replacements.reserve(targets.size());
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const auto& target = targets[i];
        const auto& temp = tempFiles[i];
        try {
            const auto bak = target.string() + ".bak";
            std::error_code ec;
            const bool existed = PathEntryExists(target, ec);
            if (ec) {
                throw std::filesystem::filesystem_error("cannot inspect save target", target, ec);
            }
            if (existed && !allowOverwrite[i]) {
                throw std::runtime_error("draft map destination already exists; refusing to overwrite it");
            }
            if (existed && !std::filesystem::is_regular_file(target)) {
                throw std::runtime_error("save target is not a regular file");
            }
            if (existed) {
                std::filesystem::copy_file(target, bak,
                                          std::filesystem::copy_options::overwrite_existing);
            } else {
                std::filesystem::remove(bak);
            }
            replacements.push_back({target, bak, existed, false});
            auto& replacement = replacements.back();
            if (existed) {
                std::filesystem::remove(target);
                replacement.changed = true;
            }
            std::filesystem::rename(temp, target);
            replacement.changed = true;
        } catch (const std::exception& ex) {
            AddIssue(issues, target.generic_string(), -1, "", IssueSeverity::Error,
                     ex.what());
            for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
                if (!it->changed) continue;
                std::error_code ec;
                if (it->existed) {
                    std::filesystem::copy_file(it->backup, it->target,
                                              std::filesystem::copy_options::overwrite_existing, ec);
                } else {
                    std::filesystem::remove(it->target, ec);
                }
                if (ec) {
                    AddIssue(issues, it->target.generic_string(), -1, "", IssueSeverity::Error,
                             "cannot restore file after failed save: " + ec.message());
                }
            }
            RemoveTempFiles(tempFiles);
            return false;
        }
    }
    // Keep backup files as transaction snapshots.
    return true;
}

} // namespace

namespace AssetPackCore {

bool PuzzleProjectStore::LoadDataFolder(
    const std::filesystem::path& selectedDataFolder, PuzzleProject& project,
    std::vector<ProjectIssue>& issues) try {
    issues.clear();
    if (!std::filesystem::is_directory(selectedDataFolder)) {
        AddIssue(issues, selectedDataFolder.generic_string(), -1, "",
                 IssueSeverity::Error, "data folder not found");
        return false;
    }

    const std::filesystem::path dataRoot = selectedDataFolder;
    const std::filesystem::path resourceRoot = dataRoot.parent_path();
    const std::filesystem::path levelsPath = dataRoot / "levels.csv";
    const std::filesystem::path nodesPath = dataRoot / "nodes.csv";
    if (!std::filesystem::is_regular_file(levelsPath) ||
        !std::filesystem::is_regular_file(nodesPath)) {
        AddIssue(issues, "levels.csv", 1, "", IssueSeverity::Error,
                 "levels.csv or nodes.csv missing");
        return false;
    }

    std::string levelsText;
    std::string nodesText;
    if (!LoadText(levelsPath, levelsText)) {
        AddIssue(issues, "levels.csv", -1, "", IssueSeverity::Error,
                 "cannot read levels.csv");
        return false;
    }
    if (!LoadText(nodesPath, nodesText)) {
        AddIssue(issues, "nodes.csv", -1, "", IssueSeverity::Error,
                 "cannot read nodes.csv");
        return false;
    }

    CsvParseResult levelsResult = CsvCodec::Parse(levelsText);
    CsvParseResult nodesResult = CsvCodec::Parse(nodesText);
    if (!levelsResult) {
        AddIssue(issues, "levels.csv", 1, "", IssueSeverity::Error,
                 "parse error: " + levelsResult.error);
        return false;
    }
    if (!nodesResult) {
        AddIssue(issues, "nodes.csv", 1, "", IssueSeverity::Error,
                 "parse error: " + nodesResult.error);
        return false;
    }

    PuzzleProject next;
    next.dataRoot = dataRoot;
    next.resourceRoot = resourceRoot;
    next.levelsPath = levelsPath;
    next.nodesPath = nodesPath;

    if (!EqualHeader(levelsResult.document->header, kLevelsHeader, "levels.csv", 1,
                    issues)) {
        return false;
    }
    if (!EqualHeader(nodesResult.document->header, kNodePresetsHeader, "nodes.csv", 1,
                    issues)) {
        return false;
    }

    std::vector<std::string> mapOrder;
    std::unordered_set<std::string> levelIds;
    std::unordered_set<std::string> presetIds;

    for (const auto& row : levelsResult.document->records) {
        ValidateAndParseLevel(row, dataRoot, resourceRoot, next.levels, mapOrder,
                             levelIds, issues);
    }
    for (const auto& row : nodesResult.document->records) {
        ValidateAndParsePreset(row, next.presets, presetIds, resourceRoot, issues);
    }

    if (AnyError(issues)) {
        return false;
    }

    for (const auto& mapPath : mapOrder) {
        const auto mapAbs = resourceRoot / std::filesystem::path{mapPath};
        std::error_code ec;
        const bool exists = PathEntryExists(mapAbs, ec);
        if (ec) {
            AddIssue(issues, mapPath, -1, "map_path", IssueSeverity::Error,
                     "cannot inspect map file: " + ec.message());
            continue;
        }
        if (!exists) {
            MapDocument mapDoc;
            mapDoc.map_path = mapPath;
            mapDoc.state = MapDocumentState::MissingFileDraft;
            mapDoc.dirty = true;
            next.maps.emplace(mapPath, std::move(mapDoc));
            next.mapLoadOrder.push_back(mapPath);
            AddIssue(issues, mapPath, -1, "map_path", IssueSeverity::Warning,
                     "map file missing; opened an editable draft that will be created on Save All");
            continue;
        }
        if (!std::filesystem::is_regular_file(mapAbs, ec)) {
            AddIssue(issues, mapPath, -1, "map_path", IssueSeverity::Error,
                     "map path is not a readable regular file");
            continue;
        }
        std::string mapText;
        if (!LoadText(mapAbs, mapText)) {
            AddIssue(issues, mapPath, -1, "map_path", IssueSeverity::Error,
                     "cannot read map file");
            continue;
        }
        CsvParseResult mapResult = CsvCodec::Parse(mapText);
        if (!mapResult) {
            AddIssue(issues, mapPath, -1, "", IssueSeverity::Error,
                     "parse error: " + mapResult.error);
            continue;
        }
        MapDocument mapDoc;
        mapDoc.map_path = mapPath;
        if (!EqualHeader(mapResult.document->header, kMapHeader, mapPath, 1, issues)) {
            continue;
        }
        for (const auto& row : mapResult.document->records) {
            ValidateAndParseMapNode(row, presetIds, resourceRoot, mapDoc, issues);
        }
        mapDoc.state = MapDocumentState::Persisted;
        mapDoc.dirty = false;
        next.maps.emplace(mapPath, std::move(mapDoc));
        next.mapLoadOrder.push_back(mapPath);
    }

    if (AnyError(issues)) {
        return false;
    }

    next.levelsDirty = false;
    next.presetsDirty = false;
    project = std::move(next);
    return true;
} catch (const std::exception& ex) {
    AddIssue(issues, selectedDataFolder.generic_string(), -1, "", IssueSeverity::Error,
             ex.what());
    return false;
}

bool PuzzleProjectStore::SaveAll(PuzzleProject& project,
                                 std::vector<ProjectIssue>& issues) try {
    issues.clear();
    if (project.dataRoot.empty() || project.resourceRoot.empty() ||
        project.levelsPath.empty() || project.nodesPath.empty()) {
        AddIssue(issues, "levels.csv", -1, "", IssueSeverity::Error,
                 "open a data folder before saving");
        return false;
    }
    if (!ParseAndValidateCurrentProject(project, issues)) {
        return false;
    }

    CsvDocument levelsDocument;
    CsvDocument presetsDocument;
    BuildLevelDocument(project, levelsDocument);
    BuildPresetDocument(project, presetsDocument);

    std::vector<std::filesystem::path> targets;
    std::vector<std::string> tempTexts;
    std::vector<bool> allowOverwrite;
    targets.push_back(project.levelsPath);
    tempTexts.push_back(CsvCodec::Serialize(levelsDocument));
    allowOverwrite.push_back(true);
    targets.push_back(project.nodesPath);
    tempTexts.push_back(CsvCodec::Serialize(presetsDocument));
    allowOverwrite.push_back(true);

    std::vector<std::string> mapOrder = project.mapLoadOrder;
    for (const auto& mapPair : project.maps) {
        if (std::find(mapOrder.begin(), mapOrder.end(), mapPair.first) == mapOrder.end()) {
            mapOrder.push_back(mapPair.first);
        }
    }
    for (const auto& mapKey : mapOrder) {
        const auto found = project.maps.find(mapKey);
        if (found == project.maps.end() || found->second.deleted) {
            continue;
        }
        CsvDocument mapDoc;
        BuildMapDocument(found->second, mapDoc);
        targets.push_back(project.resourceRoot / found->first);
        tempTexts.push_back(CsvCodec::Serialize(mapDoc));
        allowOverwrite.push_back(found->second.state == MapDocumentState::Persisted);
    }

    std::unordered_set<std::filesystem::path> savePaths;
    for (const auto& target : targets) {
        for (const auto* suffix : {"", ".tmp", ".bak"}) {
            auto path = std::filesystem::weakly_canonical(target.string() + suffix);
#ifdef _WIN32
            auto native = path.native();
            std::transform(native.begin(), native.end(), native.begin(),
                           [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
            path = std::move(native);
#endif
            if (!savePaths.insert(path).second) {
                AddIssue(issues, target.generic_string(), -1, "map_path", IssueSeverity::Error,
                         "save file paths overlap another CSV or its temporary/backup file");
                return false;
            }
        }
    }

    std::vector<std::filesystem::path> tempFiles;
    tempFiles.reserve(targets.size());
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const auto& target = targets[i];
        const auto& text = tempTexts[i];
        if (!WriteTextAtomic(target, text)) {
            AddIssue(issues, target.generic_string(), -1, "", IssueSeverity::Error,
                     "cannot write temp file");
            RemoveTempFiles(tempFiles);
            return false;
        }
        tempFiles.push_back(target.string() + ".tmp");
    }

    if (!ReplaceWrittenFiles(targets, tempFiles, allowOverwrite, issues)) {
        return false;
    }

    project.levelsDirty = false;
    project.presetsDirty = false;
    for (auto& mapPair : project.maps) {
        if (mapPair.second.deleted) continue;
        mapPair.second.state = MapDocumentState::Persisted;
        mapPair.second.dirty = false;
    }
    return true;
} catch (const std::exception& ex) {
    AddIssue(issues, project.dataRoot.generic_string(), -1, "", IssueSeverity::Error,
             ex.what());
    return false;
}

void PuzzleProjectStore::BuildIssuesFromException(std::vector<ProjectIssue>& issues,
                                                const std::string& file,
                                                const std::string& message) {
    issues.push_back(ProjectIssue{file, -1, "", IssueSeverity::Error, message});
}

} // namespace AssetPackCore
