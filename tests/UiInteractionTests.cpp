#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "app/EditorApp.hpp"
#include "ui/UiInspector.hpp"
#include "ui/UiProject.hpp"
#include "ui/UiViewport.hpp"

namespace {

namespace fs = std::filesystem;

constexpr const char* kProjectWindow = "左: レベル / マップノード / ノードひな形";
constexpr const char* kViewportWindow = "中央: Puzzle Canvas";

struct TemporaryProject {
    fs::path parent = fs::temp_directory_path().lexically_normal();
    fs::path root;

    TemporaryProject() {
        static std::atomic<unsigned> sequence = 0;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root = parent / ("assetpack_ui_" + std::to_string(stamp) + "_" +
                         std::to_string(sequence.fetch_add(1)));
        if (!fs::create_directory(root)) {
            throw std::runtime_error("could not create isolated UI test directory");
        }
        fs::create_directory(root / "data");
    }

    ~TemporaryProject() {
        // Only remove the unique fixture directory immediately inside temp.
        if (root.parent_path() == parent &&
            root.filename().string().starts_with("assetpack_ui_")) {
            std::error_code error;
            fs::remove_all(root, error);
        }
    }
};

struct ImGuiSession {
    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGuiContext* context = ImGui::CreateContext();

    ImGuiSession() {
        ImGui::SetCurrentContext(context);
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = ImVec2(2000.0f, 1600.0f);
        io.DeltaTime = 1.0f / 60.0f;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.Fonts->AddFontDefault();
        io.Fonts->Build();
        io.Fonts->SetTexID(ImTextureID{1});
    }

    ~ImGuiSession() {
        ImGui::DestroyContext(context);
        ImGui::SetCurrentContext(previous);
    }
};

struct UiFixture {
    TemporaryProject files;
    ImGuiSession session;
    App::EditorAppState state;

    UiFixture() {
        state.projectLoaded = true;
        state.project.resourceRoot = files.root;
        state.project.dataRoot = files.root / "data";
        state.project.levelsPath = state.project.dataRoot / "levels.csv";
        state.project.nodesPath = state.project.dataRoot / "nodes.csv";

        AssetPackCore::NodePresetRow preset;
        preset.preset_id = "root";
        preset.node_type = "root";
        preset.width_tiles = "1";
        preset.height_tiles = "1";
        state.project.presets.push_back(preset);
    }

    void Frame() {
        ImGui::SetCurrentContext(session.context);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(600.0f, 1500.0f), ImGuiCond_Always);
        UI::DrawProjectPanel(state);
        ImGui::SetNextWindowPos(ImVec2(650.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(900.0f, 700.0f), ImGuiCond_Always);
        UI::DrawViewportPanel(state, nullptr);
        ImGui::SetNextWindowPos(ImVec2(1560.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(400.0f, 1000.0f), ImGuiCond_Always);
        UI::DrawInspectorPanel(state);
        ImGui::Render();
    }

    void Activate(ImGuiID id) {
        ImGui::ActivateItemByID(id);
        Frame();
        Frame();
    }

    void ActivateSectionButton(const char* section, const char* label) {
        auto* window = ImGui::FindWindowByName(kProjectWindow);
        REQUIRE(window != nullptr);
        const ImGuiID sectionId = ImHashStr(section, 0, window->ID);
        Activate(ImHashStr(label, 0, sectionId));
    }

    void ActivateListRow(const char* listName, const char* label) {
        auto* parent = ImGui::FindWindowByName(kProjectWindow);
        REQUIRE(parent != nullptr);
        ImGuiWindow* child = nullptr;
        const std::string marker = std::string("/") + listName + "_";
        for (auto* window : GImGui->Windows) {
            if (window->ParentWindow == parent &&
                std::strstr(window->Name, marker.c_str()) != nullptr) {
                child = window;
                break;
            }
        }
        REQUIRE(child != nullptr);
        Activate(child->GetID(label));
    }

    void SelectLevelAndPreset() {
        REQUIRE(App::EditorApp::CreateLevel(state, "draft", "Draft"));
        App::EditorApp::SelectLevel(state, -1);
        Frame();
        Frame();
        ActivateListRow("level_list", "draft");
        REQUIRE(state.selectedLevelIndex == 0);
        ActivateListRow("preset_list", "root (root)");
        REQUIRE(state.selectedPresetIndex == 0);
        REQUIRE(state.selectedLevelIndex == 0);
        REQUIRE(state.selectedTarget == App::SelectTarget::Preset);
        REQUIRE(state.SelectedMap() != nullptr);
    }

    ImVec2 TileCenter(int x, int y) const {
        const auto* window = ImGui::FindWindowByName(kViewportWindow);
        REQUIRE(window != nullptr);
        const float maximum = std::numeric_limits<float>::max();
        ImVec2 minimum(maximum, maximum);
        ImVec2 extent(-maximum, -maximum);
        bool found = false;
        // Read the rendered canvas bounds instead of duplicating UI layout math.
        for (const auto& vertex : window->DrawList->VtxBuffer) {
            if (vertex.col != IM_COL32(18, 18, 26, 255)) continue;
            minimum.x = std::min(minimum.x, vertex.pos.x);
            minimum.y = std::min(minimum.y, vertex.pos.y);
            extent.x = std::max(extent.x, vertex.pos.x);
            extent.y = std::max(extent.y, vertex.pos.y);
            found = true;
        }
        REQUIRE(found);
        REQUIRE(extent.x > minimum.x);
        REQUIRE(extent.y > minimum.y);
        return ImVec2(minimum.x + (x + 0.5f) * (extent.x - minimum.x) / 80.0f,
                      minimum.y + (y + 0.5f) * (extent.y - minimum.y) / 45.0f);
    }

    void MoveMouse(const ImVec2& position) {
        ImGui::GetIO().AddMousePosEvent(position.x, position.y);
        Frame();
    }

    void MouseButton(bool down) {
        ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, down);
        Frame();
    }

    void AddNode(int x, int y) {
        ActivateSectionButton("map_nodes", "マップへ追加");
        REQUIRE(state.isAddMode);
        MoveMouse(TileCenter(x, y));
        MouseButton(true);
        MouseButton(false);
        REQUIRE(state.SelectedMap() != nullptr);
        REQUIRE_FALSE(state.SelectedMap()->nodes.empty());
    }

    void CheckNoMapFiles() const {
        for (const auto& [path, map] : state.project.maps) {
            CHECK_FALSE(fs::exists(state.project.resourceRoot / path));
        }
        CHECK_FALSE(fs::exists(state.project.dataRoot / "maps"));
    }
};

TEST_CASE("UI keeps the draft map active while selecting and placing a preset") {
    UiFixture fixture;
    fixture.SelectLevelAndPreset();
    fixture.CheckNoMapFiles();
    fixture.AddNode(3, 4);

    auto* map = fixture.state.SelectedMap();
    REQUIRE(map != nullptr);
    REQUIRE(map->nodes.size() == 1);
    CHECK(map->nodes[0].source_preset_id == "root");
    CHECK(map->nodes[0].tile_x.value == "3");
    CHECK(map->nodes[0].tile_y.value == "4");
    CHECK(map->dirty);
    CHECK(fixture.state.selectedTarget == App::SelectTarget::MapNode);
    CHECK(fixture.state.selectedMapNodeIndex == 0);
    CHECK(fixture.state.selectedPresetIndex == 0);
    CHECK_FALSE(fixture.state.isAddMode);

    fixture.MoveMouse(fixture.TileCenter(3, 4));
    fixture.MouseButton(true);
    fixture.MoveMouse(fixture.TileCenter(7, 8));
    fixture.MouseButton(false);
    CHECK(map->nodes[0].tile_x.value == "7");
    CHECK(map->nodes[0].tile_y.value == "8");
    CHECK_FALSE(fixture.state.draggingNode);
    fixture.CheckNoMapFiles();
}

TEST_CASE("same-label duplicate buttons affect only their own UI section") {
    UiFixture fixture;
    fixture.SelectLevelAndPreset();
    fixture.AddNode(2, 2);
    const std::string originalMapPath = fixture.state.SelectedMap()->map_path;

    fixture.ActivateSectionButton("map_nodes", "複製");
    REQUIRE(fixture.state.SelectedMap() != nullptr);
    CHECK(fixture.state.SelectedMap()->nodes.size() == 2);
    CHECK(fixture.state.project.levels.size() == 1);
    CHECK(fixture.state.project.presets.size() == 1);

    fixture.ActivateSectionButton("levels", "複製");
    REQUIRE(fixture.state.SelectedMap() != nullptr);
    CHECK(fixture.state.project.levels.size() == 2);
    CHECK(fixture.state.project.presets.size() == 1);
    CHECK(fixture.state.SelectedMap()->map_path != originalMapPath);
    CHECK(fixture.state.SelectedMap()->nodes.size() == 2);
    CHECK(fixture.state.project.maps.at(originalMapPath).nodes.size() == 2);

    fixture.ActivateSectionButton("presets", "複製");
    CHECK(fixture.state.project.levels.size() == 2);
    CHECK(fixture.state.project.presets.size() == 2);
    CHECK(fixture.state.SelectedMap()->nodes.size() == 2);
    fixture.CheckNoMapFiles();
}

}  // namespace
