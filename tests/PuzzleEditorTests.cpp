#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "app/EditorApp.hpp"
#include "assetpack_core/PuzzleResolver.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

namespace fs = std::filesystem;
using App::EditorApp;
using App::EditorAppState;
using App::SelectTarget;
using namespace AssetPackCore;

constexpr const char* kLevelsHeader =
    "level_id,level_name,map_path,next_level_id,total_length,minimum_slack_ratio,"
    "background_color,vessel_color,base_width,tip_width,width_variation\n";
constexpr const char* kPresetsCsv =
    "preset_id,node_type,texture_path,width_tiles,height_tiles,display_name,"
    "max_incoming,max_outgoing,max_outgoing_length\n"
    "preset_a,follow,,1,1,Follow,1,2,8\n"
    "preset_b,end,,2,1,End,2,0,0\n";
constexpr const char* kMapHeader =
    "instance_id,source_preset_id,node_type,texture_path,width_tiles,height_tiles,"
    "display_name,tile_x,tile_y,max_incoming,max_outgoing,max_outgoing_length\n";

struct LevelFixtureRow {
    std::string id;
    std::string name;
    std::string mapPath;
};

// Every fixture owns one uniquely created directory directly below the system
// temporary directory. Cleanup checks that boundary before removing anything.
class ProjectFixture {
public:
    explicit ProjectFixture(const std::string& folderName = "data") {
        static std::atomic<unsigned long long> sequence{0};
        tempParent_ = fs::weakly_canonical(fs::temp_directory_path());
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            const auto name = "assetpack_editor_test_" + std::to_string(stamp) +
                              "_" + std::to_string(sequence.fetch_add(1));
            const auto candidate = tempParent_ / name;
            if (fs::create_directory(candidate)) {
                root = fs::weakly_canonical(candidate);
                break;
            }
        }
        if (root.empty()) throw std::runtime_error("cannot create test fixture");
        data = root / folderName;
        Write(data / "levels.csv", kLevelsHeader);
        Write(data / "nodes.csv", kPresetsCsv);
    }

    ~ProjectFixture() {
        std::error_code ec;
        const auto resolved = fs::weakly_canonical(root, ec);
        if (!ec && !root.empty() && resolved == root &&
            resolved.parent_path() == tempParent_ &&
            resolved.filename().string().starts_with("assetpack_editor_test_")) {
            fs::remove_all(resolved, ec);
        }
    }

    ProjectFixture(const ProjectFixture&) = delete;
    ProjectFixture& operator=(const ProjectFixture&) = delete;

    void WriteLevels(std::initializer_list<LevelFixtureRow> rows) const {
        std::string csv = kLevelsHeader;
        for (const auto& row : rows) {
            csv += row.id + "," + row.name + "," + row.mapPath + ",,,,,,,,\n";
        }
        Write(data / "levels.csv", csv);
    }

    static void Write(const fs::path& path, const std::string& text) {
        fs::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << text;
        output.close();
        if (!output) throw std::runtime_error("cannot write test fixture");
    }

    static std::string Read(const fs::path& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("cannot read test fixture");
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    fs::path root;
    fs::path data;

private:
    fs::path tempParent_;
};

bool HasIssue(const EditorAppState& state, IssueSeverity severity) {
    return std::any_of(state.issues.begin(), state.issues.end(),
                       [severity](const ProjectIssue& issue) {
                           return issue.severity == severity;
                       });
}

void LoadFixture(EditorApp& app, EditorAppState& state, const ProjectFixture& fixture) {
    REQUIRE(app.LoadDataFolder(state, fixture.data));
    REQUIRE(state.projectLoaded);
}

void AddNode(EditorAppState& state, int x = 3, int y = 4) {
    EditorApp::SelectPreset(state, 0);
    REQUIRE(EditorApp::BeginAddNode(state));
    REQUIRE(EditorApp::PlaceNode(state, x, y));
}

void CheckOverride(const OverrideField& actual, const OverrideField& expected) {
    CHECK(actual.overridden == expected.overridden);
    CHECK(actual.value == expected.value);
}

void CheckNode(const MapNodeRow& actual, const MapNodeRow& expected) {
    CHECK(actual.instance_id == expected.instance_id);
    CHECK(actual.source_preset_id == expected.source_preset_id);
    CheckOverride(actual.node_type, expected.node_type);
    CheckOverride(actual.texture_path, expected.texture_path);
    CheckOverride(actual.width_tiles, expected.width_tiles);
    CheckOverride(actual.height_tiles, expected.height_tiles);
    CheckOverride(actual.display_name, expected.display_name);
    CheckOverride(actual.tile_x, expected.tile_x);
    CheckOverride(actual.tile_y, expected.tile_y);
    CheckOverride(actual.max_incoming, expected.max_incoming);
    CheckOverride(actual.max_outgoing, expected.max_outgoing);
    CheckOverride(actual.max_outgoing_length, expected.max_outgoing_length);
}

} // namespace

TEST_CASE("new levels remain editable in memory until SaveAll, then round trip") {
    ProjectFixture fixture;
    EditorApp app;
    EditorAppState state;
    LoadFixture(app, state, fixture);
    REQUIRE(EditorApp::CreateLevel(state, "alpha", "Alpha"));
    REQUIRE(state.selectedLevelIndex == 0);
    CHECK(state.selectedTarget == SelectTarget::Level);
    REQUIRE(state.SelectedMap() != nullptr);
    const auto mapPath = state.project.levels[0].map_path;
    CHECK(mapPath == "data/maps/alpha.csv");
    CHECK(state.SelectedMap()->state == MapDocumentState::NewDraft);
    CHECK(state.SelectedMap()->dirty);
    CHECK(state.project.levelsDirty);
    CHECK_FALSE(fs::exists(fixture.data / "maps"));

    AddNode(state);
    REQUIRE(state.SelectedMap()->nodes.size() == 1);
    CHECK(state.selectedLevelIndex == 0);
    CHECK(state.selectedPresetIndex == 0);
    CHECK(state.selectedTarget == SelectTarget::MapNode);
    CHECK_FALSE(state.isAddMode);
    CHECK(state.selectedPresetForAdd.empty());
    CHECK_FALSE(EditorApp::PlaceNode(state, 5, 6));
    REQUIRE(state.SelectedMap()->nodes.size() == 1);

    auto& node = state.SelectedMap()->nodes.front();
    CHECK(node.tile_x.overridden);
    CHECK(node.tile_y.overridden);
    CHECK(node.tile_x.value == "3");
    CHECK(node.tile_y.value == "4");
    node.display_name = {true, "Edited, \"quoted\" node"};
    node.width_tiles = {true, "3"};
    node.tile_x = {true, "11"};
    node.tile_y = {true, "12"};
    state.SelectedMap()->dirty = true;
    const auto expectedNode = node;

    REQUIRE(EditorApp::CreateLevel(state, "beta", "Beta"));
    REQUIRE(state.SelectedMap() != nullptr);
    CHECK(state.SelectedMap()->nodes.empty());
    EditorApp::SelectLevel(state, 0);
    REQUIRE(state.SelectedMap() != nullptr);
    REQUIRE(state.SelectedMap()->nodes.size() == 1);
    CheckNode(state.SelectedMap()->nodes.front(), expectedNode);
    CHECK_FALSE(fs::exists(fixture.data / "maps"));
    CHECK(ProjectFixture::Read(fixture.data / "levels.csv") == kLevelsHeader);

    REQUIRE(app.SaveAll(state));
    CHECK_FALSE(state.project.HasDirty());
    CHECK(fs::is_regular_file(fixture.root / mapPath));
    for (const auto& [key, map] : state.project.maps) {
        CHECK(map.state == MapDocumentState::Persisted);
        CHECK_FALSE(map.dirty);
        CHECK(fs::is_regular_file(fixture.root / key));
    }

    EditorAppState reloaded;
    LoadFixture(app, reloaded, fixture);
    REQUIRE(reloaded.project.levels.size() == 2);
    EditorApp::SelectLevel(reloaded, 0);
    REQUIRE(reloaded.SelectedMap() != nullptr);
    REQUIRE(reloaded.SelectedMap()->nodes.size() == 1);
    CheckNode(reloaded.SelectedMap()->nodes.front(), expectedNode);
    const auto resolved = PuzzleResolver::Resolve(reloaded.project.presets[0],
                                                  reloaded.SelectedMap()->nodes[0]);
    CHECK(resolved.has_placement);
    CHECK(resolved.tile_x == 11);
    CHECK(resolved.tile_y == 12);
    CHECK(resolved.width_tiles == 3);
    CHECK_FALSE(reloaded.project.HasDirty());
}

TEST_CASE("levels referring to an existing map share one persisted document") {
    ProjectFixture fixture;
    fixture.WriteLevels({{"first", "First", "data/maps/shared.csv"},
                         {"second", "Second", "data/maps/shared.csv"}});
    ProjectFixture::Write(fixture.data / "maps/shared.csv",
                          std::string(kMapHeader) +
                          "node_a,preset_a,,,,,Loaded,2,3,,,\n");
    EditorApp app;
    EditorAppState state;
    LoadFixture(app, state, fixture);
    REQUIRE(state.project.maps.size() == 1);
    CHECK(state.project.mapLoadOrder.size() == 1);
    CHECK_FALSE(state.project.HasDirty());
    EditorApp::SelectLevel(state, 0);
    auto* firstMap = state.SelectedMap();
    REQUIRE(firstMap != nullptr);
    CHECK(firstMap->state == MapDocumentState::Persisted);
    REQUIRE(firstMap->nodes.size() == 1);
    firstMap->nodes[0].display_name = {true, "Shared edit"};
    firstMap->dirty = true;
    EditorApp::SelectLevel(state, 1);
    REQUIRE(state.SelectedMap() == firstMap);
    CHECK(state.SelectedMap()->nodes[0].display_name.value == "Shared edit");
    REQUIRE(app.SaveAll(state));
    CHECK_FALSE(state.project.HasDirty());
    CHECK(state.project.levels[0].map_path == "data/maps/shared.csv");
    CHECK(state.project.levels[1].map_path == "data/maps/shared.csv");
}

TEST_CASE("missing maps become warned repair drafts without creating files") {
    ProjectFixture fixture;
    fixture.WriteLevels({{"repair", "Repair", "data/maps/original.csv"}});
    EditorApp app;
    EditorAppState state;
    LoadFixture(app, state, fixture);
    REQUIRE(state.project.maps.size() == 1);
    EditorApp::SelectLevel(state, 0);
    REQUIRE(state.SelectedMap() != nullptr);
    CHECK(state.SelectedMap()->state == MapDocumentState::MissingFileDraft);
    CHECK(state.SelectedMap()->dirty);
    CHECK(state.project.HasDirty());
    CHECK_FALSE(state.project.levelsDirty);
    CHECK(HasIssue(state, IssueSeverity::Warning));
    CHECK_FALSE(HasIssue(state, IssueSeverity::Error));
    CHECK_FALSE(fs::exists(fixture.data / "maps"));

    SUBCASE("saving an empty repair draft creates its original map") {}
    SUBCASE("repair drafts support node placement before saving") {
        AddNode(state, 7, 8);
        REQUIRE(state.SelectedMap()->nodes.size() == 1);
        CHECK(state.SelectedMap()->nodes[0].tile_x.value == "7");
        CHECK_FALSE(fs::exists(fixture.data / "maps"));
    }

    const auto expectedCount = state.SelectedMap()->nodes.size();
    REQUIRE(app.SaveAll(state));
    CHECK(state.SelectedMap()->state == MapDocumentState::Persisted);
    CHECK_FALSE(state.project.HasDirty());
    CHECK(fs::is_regular_file(fixture.data / "maps/original.csv"));
    EditorAppState reloaded;
    LoadFixture(app, reloaded, fixture);
    EditorApp::SelectLevel(reloaded, 0);
    REQUIRE(reloaded.SelectedMap() != nullptr);
    CHECK(reloaded.SelectedMap()->nodes.size() == expectedCount);
    CHECK(reloaded.SelectedMap()->state == MapDocumentState::Persisted);
    CHECK_FALSE(HasIssue(reloaded, IssueSeverity::Warning));
}

TEST_CASE("an existing invalid map fails loading and preserves the open project") {
    ProjectFixture fixture;
    EditorApp app;
    EditorAppState state;
    LoadFixture(app, state, fixture);
    REQUIRE(EditorApp::CreateLevel(state, "unsaved", "Unsaved"));
    AddNode(state);
    const auto originalRoot = state.project.dataRoot;
    const auto originalMapPath = state.project.levels[0].map_path;
    const auto originalNode = state.SelectedMap()->nodes[0];

    ProjectFixture invalid;
    invalid.WriteLevels({{"broken", "Broken", "data/maps/broken.csv"}});
    const auto brokenPath = invalid.data / "maps/broken.csv";
    SUBCASE("wrong map header") {
        ProjectFixture::Write(brokenPath, "wrong,header\n");
    }
    SUBCASE("malformed CSV") {
        ProjectFixture::Write(brokenPath, std::string(kMapHeader) + "\"unfinished");
    }
    SUBCASE("invalid map node") {
        ProjectFixture::Write(brokenPath,
                              std::string(kMapHeader) +
                              "node_a,unknown_preset,,,,,,2,3,,,\n");
    }
    SUBCASE("existing path is a directory rather than a missing file") {
        fs::create_directories(brokenPath);
    }

    CHECK_FALSE(app.LoadDataFolder(state, invalid.data));
    CHECK(HasIssue(state, IssueSeverity::Error));
    CHECK(state.projectLoaded);
    CHECK(state.project.dataRoot == originalRoot);
    REQUIRE(state.project.levels.size() == 1);
    CHECK(state.project.levels[0].level_id == "unsaved");
    CHECK(state.project.levels[0].map_path == originalMapPath);
    REQUIRE(state.SelectedMap() != nullptr);
    REQUIRE(state.SelectedMap()->nodes.size() == 1);
    CheckNode(state.SelectedMap()->nodes[0], originalNode);
    CHECK(state.project.HasDirty());
    CHECK_FALSE(fs::exists(fixture.data / "maps"));
}

TEST_CASE("duplicating a level survives vector growth and owns a separate draft") {
    ProjectFixture fixture;
    EditorApp app;
    EditorAppState state;
    LoadFixture(app, state, fixture);
    REQUIRE(EditorApp::CreateLevel(state, "source", "Source"));
    AddNode(state);
    auto& sourceLevel = state.project.levels[0];
    sourceLevel.total_length = "120.5";
    sourceLevel.minimum_slack_ratio = "0.2";
    sourceLevel.background_color = "#112233";
    sourceLevel.vessel_color = "#44556677";
    sourceLevel.base_width = "8";
    sourceLevel.tip_width = "2";
    sourceLevel.width_variation = "0.3";
    sourceLevel.next_level_id = "source";
    state.SelectedMap()->nodes[0].display_name = {true, std::string(200, 'x')};
    state.SelectedMap()->nodes[0].max_outgoing_length = {true, "10.5"};
    const auto expectedLevel = sourceLevel;
    const auto expectedNode = state.SelectedMap()->nodes[0];
    const auto sourcePath = sourceLevel.map_path;

    // Force the duplicate append to reallocate, exposing stale source references.
    while (state.project.levels.size() < state.project.levels.capacity()) {
        REQUIRE(EditorApp::CreateLevel(state, "filler", "Filler"));
    }
    const auto previousCapacity = state.project.levels.capacity();
    const auto previousCount = state.project.levels.size();
    EditorApp::SelectLevel(state, 0);
    REQUIRE(EditorApp::DuplicateSelectedLevel(state));
    CHECK(state.project.levels.capacity() > previousCapacity);
    REQUIRE(state.project.levels.size() == previousCount + 1);
    REQUIRE(state.selectedLevelIndex == static_cast<int>(previousCount));
    const auto& copy = state.project.levels.back();
    CHECK(copy.level_id != expectedLevel.level_id);
    CHECK(copy.map_path != sourcePath);
    CHECK(copy.next_level_id == expectedLevel.next_level_id);
    CHECK(copy.total_length == expectedLevel.total_length);
    CHECK(copy.minimum_slack_ratio == expectedLevel.minimum_slack_ratio);
    CHECK(copy.background_color == expectedLevel.background_color);
    CHECK(copy.vessel_color == expectedLevel.vessel_color);
    CHECK(copy.base_width == expectedLevel.base_width);
    CHECK(copy.tip_width == expectedLevel.tip_width);
    CHECK(copy.width_variation == expectedLevel.width_variation);
    REQUIRE(state.SelectedMap() != nullptr);
    CHECK(state.SelectedMap()->state == MapDocumentState::NewDraft);
    CHECK(state.SelectedMap()->dirty);
    REQUIRE(state.SelectedMap()->nodes.size() == 1);
    CheckNode(state.SelectedMap()->nodes[0], expectedNode);
    state.SelectedMap()->nodes[0].display_name = {true, "Copy only"};
    state.SelectedMap()->nodes[0].tile_x = {true, "20"};
    const auto copiedPath = copy.map_path;
    EditorApp::SelectLevel(state, 0);
    CheckNode(state.SelectedMap()->nodes[0], expectedNode);
    CHECK_FALSE(fs::exists(fixture.data / "maps"));
    REQUIRE(app.SaveAll(state));
    CHECK(fs::is_regular_file(fixture.root / sourcePath));
    CHECK(fs::is_regular_file(fixture.root / copiedPath));
}

TEST_CASE("new map paths respect the selected folder and avoid existing names") {
    ProjectFixture fixture("campaigns");
    EditorApp app;
    EditorAppState state;
    LoadFixture(app, state, fixture);
    const std::string reservedPath = "campaigns/maps/alpha.csv";
    bool fileReserved = false;

    SUBCASE("custom folder name is retained") {}
    SUBCASE("a name already used by an in-memory document is skipped") {
        MapDocument reserved;
        reserved.map_path = reservedPath;
        state.project.maps.emplace(reservedPath, reserved);
    }
    SUBCASE("a filename already on disk is skipped") {
        ProjectFixture::Write(fixture.root / reservedPath, "existing map content");
        fileReserved = true;
    }

    const bool occupied = fileReserved || state.project.maps.contains(reservedPath);
    REQUIRE(EditorApp::CreateLevel(state, "alpha", "Alpha"));
    const auto firstId = state.project.levels.back().level_id;
    const auto firstPath = state.project.levels.back().map_path;
    CHECK(fs::path(firstPath).parent_path().generic_string() == "campaigns/maps");
    CHECK(fs::path(firstPath).is_relative());
    CHECK_FALSE(fs::exists(fixture.root / firstPath));
    if (occupied) CHECK(firstPath != reservedPath);
    if (fileReserved) {
        CHECK(ProjectFixture::Read(fixture.root / reservedPath) == "existing map content");
    }
    REQUIRE(EditorApp::CreateLevel(state, "alpha", "Another alpha"));
    CHECK(state.project.levels.back().level_id != firstId);
    CHECK(state.project.levels.back().map_path != firstPath);
    CHECK(fs::path(state.project.levels.back().map_path).parent_path().generic_string() ==
          "campaigns/maps");
    REQUIRE(app.SaveAll(state));
    CHECK(fs::is_regular_file(fixture.root / firstPath));
    if (fileReserved) {
        CHECK(ProjectFixture::Read(fixture.root / reservedPath) == "existing map content");
    }
}

TEST_CASE("draft save conflicts preserve edits and never overwrite arriving files") {
    ProjectFixture fixture;
    EditorApp app;
    EditorAppState state;
    SUBCASE("new draft conflict") {
        LoadFixture(app, state, fixture);
        REQUIRE(EditorApp::CreateLevel(state, "draft", "Draft"));
    }
    SUBCASE("missing file repair conflict") {
        fixture.WriteLevels({{"draft", "Draft", "data/maps/draft.csv"}});
        LoadFixture(app, state, fixture);
        EditorApp::SelectLevel(state, 0);
    }
    AddNode(state);
    const auto mapPath = state.project.levels[0].map_path;
    const auto previousState = state.SelectedMap()->state;
    const auto originalLevels = ProjectFixture::Read(fixture.data / "levels.csv");
    const auto originalPresets = ProjectFixture::Read(fixture.data / "nodes.csv");
    ProjectFixture::Write(fixture.root / mapPath, "arrived after draft creation");
    CHECK_FALSE(app.SaveAll(state));
    CHECK(HasIssue(state, IssueSeverity::Error));
    CHECK(ProjectFixture::Read(fixture.root / mapPath) == "arrived after draft creation");
    CHECK(ProjectFixture::Read(fixture.data / "levels.csv") == originalLevels);
    CHECK(ProjectFixture::Read(fixture.data / "nodes.csv") == originalPresets);
    REQUIRE(state.SelectedMap() != nullptr);
    CHECK(state.SelectedMap()->state == previousState);
    CHECK(state.SelectedMap()->dirty);
    REQUIRE(state.SelectedMap()->nodes.size() == 1);
    CHECK(state.SelectedMap()->nodes[0].tile_x.value == "3");
    CHECK(state.project.HasDirty());
}

TEST_CASE("SaveAll validates map associations before writing project files") {
    ProjectFixture fixture;
    EditorApp app;
    EditorAppState state;
    LoadFixture(app, state, fixture);
    REQUIRE(EditorApp::CreateLevel(state, "draft", "Draft"));
    const auto originalLevels = ProjectFixture::Read(fixture.data / "levels.csv");
    const auto originalPresets = ProjectFixture::Read(fixture.data / "nodes.csv");
    SUBCASE("level references no in-memory document") {
        state.project.maps.clear();
    }
    SUBCASE("map key and document path disagree") {
        state.SelectedMap()->map_path = "data/maps/different.csv";
    }
    SUBCASE("referenced map is deleted") {
        state.SelectedMap()->deleted = true;
    }
    SUBCASE("level path escapes the selected data folder") {
        state.project.levels[0].map_path = "outside.csv";
    }
    SUBCASE("map content is invalid") {
        AddNode(state);
        state.SelectedMap()->nodes[0].source_preset_id = "does_not_exist";
    }
    CHECK_FALSE(app.SaveAll(state));
    CHECK(HasIssue(state, IssueSeverity::Error));
    CHECK(state.project.levelsDirty);
    CHECK(state.project.HasDirty());
    CHECK(ProjectFixture::Read(fixture.data / "levels.csv") == originalLevels);
    CHECK(ProjectFixture::Read(fixture.data / "nodes.csv") == originalPresets);
    CHECK_FALSE(fs::exists(fixture.data / "maps"));
}

TEST_CASE("a blocked save path leaves draft state and existing files intact") {
    ProjectFixture fixture;
    EditorApp app;
    EditorAppState state;
    LoadFixture(app, state, fixture);
    REQUIRE(EditorApp::CreateLevel(state, "blocked", "Blocked"));
    AddNode(state);
    const auto originalLevels = ProjectFixture::Read(fixture.data / "levels.csv");
    const auto originalPresets = ProjectFixture::Read(fixture.data / "nodes.csv");
    // A regular file where a directory is needed fails portably, including when
    // tests run with privileges that would bypass ordinary permission bits.
    ProjectFixture::Write(fixture.data / "maps", "directory blocker");
    CHECK_FALSE(app.SaveAll(state));
    CHECK(HasIssue(state, IssueSeverity::Error));
    CHECK(state.project.HasDirty());
    CHECK(state.project.levelsDirty);
    REQUIRE(state.SelectedMap() != nullptr);
    CHECK(state.SelectedMap()->state == MapDocumentState::NewDraft);
    CHECK(state.SelectedMap()->dirty);
    REQUIRE(state.SelectedMap()->nodes.size() == 1);
    CHECK(ProjectFixture::Read(fixture.data / "maps") == "directory blocker");
    CHECK(ProjectFixture::Read(fixture.data / "levels.csv") == originalLevels);
    CHECK(ProjectFixture::Read(fixture.data / "nodes.csv") == originalPresets);
}

TEST_CASE("staging and replacement failures preserve all pending edits") {
    ProjectFixture fixture;
    EditorApp app;
    EditorAppState state;
    LoadFixture(app, state, fixture);
    REQUIRE(EditorApp::CreateLevel(state, "pending", "Pending"));
    AddNode(state);
    state.project.presets[0].display_name = "Unsaved preset change";
    state.project.presetsDirty = true;
    const auto originalLevels = ProjectFixture::Read(fixture.data / "levels.csv");
    const auto originalPresets = ProjectFixture::Read(fixture.data / "nodes.csv");
    const auto mapTarget = fixture.root / state.project.levels[0].map_path;
    fs::path blocker;
    SUBCASE("map staging fails after configuration files have been staged") {
        blocker = fs::path(mapTarget.string() + ".tmp") / "keep.txt";
    }
    SUBCASE("map replacement fails after configuration files have been replaced") {
        blocker = fs::path(mapTarget.string() + ".bak") / "keep.txt";
    }
    ProjectFixture::Write(blocker, "blocker must remain");
    CHECK_FALSE(app.SaveAll(state));
    CHECK(HasIssue(state, IssueSeverity::Error));
    CHECK(state.project.levelsDirty);
    CHECK(state.project.presetsDirty);
    CHECK(state.project.presets[0].display_name == "Unsaved preset change");
    REQUIRE(state.SelectedMap() != nullptr);
    CHECK(state.SelectedMap()->state == MapDocumentState::NewDraft);
    CHECK(state.SelectedMap()->dirty);
    REQUIRE(state.SelectedMap()->nodes.size() == 1);
    CHECK(state.SelectedMap()->nodes[0].tile_x.value == "3");
    CHECK_FALSE(fs::exists(mapTarget));
    CHECK_FALSE(fs::exists(fixture.data / "levels.csv.tmp"));
    CHECK_FALSE(fs::exists(fixture.data / "nodes.csv.tmp"));
    CHECK(ProjectFixture::Read(fixture.data / "levels.csv") == originalLevels);
    CHECK(ProjectFixture::Read(fixture.data / "nodes.csv") == originalPresets);
    CHECK(ProjectFixture::Read(blocker) == "blocker must remain");
}

TEST_CASE("level and preset selection work in either order") {
    ProjectFixture fixture;
    EditorApp app;
    EditorAppState state;
    LoadFixture(app, state, fixture);
    REQUIRE(EditorApp::CreateLevel(state, "alpha", "Alpha"));
    app.ClearSelection(state);
    SUBCASE("level then preset keeps the selected map") {
        EditorApp::SelectLevel(state, 0);
        EditorApp::SelectPreset(state, 1);
        CHECK(state.selectedTarget == SelectTarget::Preset);
    }
    SUBCASE("preset then level keeps the selected preset") {
        EditorApp::SelectPreset(state, 1);
        EditorApp::SelectLevel(state, 0);
        CHECK(state.selectedTarget == SelectTarget::Level);
    }
    CHECK(state.selectedLevelIndex == 0);
    CHECK(state.selectedPresetIndex == 1);
    REQUIRE(state.SelectedMap() != nullptr);
    REQUIRE(EditorApp::BeginAddNode(state));
    CHECK(state.selectedPresetForAdd == "preset_b");
    REQUIRE(EditorApp::PlaceNode(state, 1, 2));
    REQUIRE(state.SelectedMap()->nodes.size() == 1);
    CHECK(state.SelectedMap()->nodes[0].source_preset_id == "preset_b");
}

TEST_CASE("switching a level cancels pending placement and dragging") {
    ProjectFixture fixture;
    EditorApp app;
    EditorAppState state;
    LoadFixture(app, state, fixture);
    REQUIRE(EditorApp::CreateLevel(state, "alpha", "Alpha"));
    AddNode(state);
    REQUIRE(EditorApp::CreateLevel(state, "beta", "Beta"));
    EditorApp::SelectLevel(state, 0);
    EditorApp::SelectPreset(state, 0);
    REQUIRE(EditorApp::BeginAddNode(state));
    state.draggingNode = true;
    state.draggingNodeIndex = 0;
    state.selectedMapNodeIndex = 0;
    state.selectedTarget = SelectTarget::MapNode;
    EditorApp::SelectLevel(state, 1);
    CHECK(state.selectedLevelIndex == 1);
    CHECK(state.selectedPresetIndex == 0);
    CHECK(state.selectedTarget == SelectTarget::Level);
    CHECK(state.selectedMapNodeIndex == -1);
    CHECK_FALSE(state.draggingNode);
    CHECK(state.draggingNodeIndex == -1);
    CHECK_FALSE(state.isAddMode);
    CHECK(state.selectedPresetForAdd.empty());
    CHECK_FALSE(EditorApp::PlaceNode(state, 10, 10));
    REQUIRE(state.SelectedMap() != nullptr);
    CHECK(state.SelectedMap()->nodes.empty());
    EditorApp::SelectLevel(state, 0);
    REQUIRE(state.SelectedMap()->nodes.size() == 1);
    CHECK(state.SelectedMap()->nodes[0].tile_x.value == "3");
}

TEST_CASE("map lookup never creates missing documents") {
    EditorAppState state;
    state.projectLoaded = true;
    LevelRow level;
    level.level_id = "unresolved";
    level.level_name = "Unresolved";
    level.map_path = "data/maps/unresolved.csv";
    state.project.levels.push_back(level);
    state.selectedLevelIndex = 0;
    const EditorAppState& constState = state;
    CHECK(state.SelectedMap() == nullptr);
    CHECK(constState.SelectedMap() == nullptr);
    CHECK(state.project.maps.empty());
    CHECK(state.project.mapLoadOrder.empty());
    state.selectedLevelIndex = -1;
    CHECK(state.SelectedMap() == nullptr);
    state.selectedLevelIndex = 5;
    CHECK(constState.SelectedMap() == nullptr);
    CHECK(state.project.maps.empty());
}

TEST_CASE("invalid creation or add requests do not produce partial documents") {
    ProjectFixture fixture;
    EditorApp app;
    EditorAppState state;
    CHECK_FALSE(EditorApp::CreateLevel(state, "alpha", "Alpha"));
    CHECK(state.project.levels.empty());
    LoadFixture(app, state, fixture);
    CHECK_FALSE(EditorApp::CreateLevel(state, "../escape", "Escape"));
    CHECK_FALSE(EditorApp::CreateLevel(state, "alpha", ""));
    CHECK(state.project.levels.empty());
    CHECK(state.project.maps.empty());
    CHECK_FALSE(EditorApp::BeginAddNode(state));
    CHECK_FALSE(EditorApp::DuplicateSelectedLevel(state));
    REQUIRE(EditorApp::CreateLevel(state, "alpha", "Alpha"));
    CHECK_FALSE(EditorApp::BeginAddNode(state));
    EditorApp::SelectPreset(state, 0);
    REQUIRE(EditorApp::BeginAddNode(state));
    CHECK_FALSE(EditorApp::PlaceNode(state, -1, 0));
    REQUIRE(state.SelectedMap() != nullptr);
    CHECK(state.SelectedMap()->nodes.empty());
    CHECK_FALSE(fs::exists(fixture.data / "maps"));
}
