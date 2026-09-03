#include "UiRoot.hpp"
#include <imgui.h>
#include <imgui_internal.h>

#include "UiProject.hpp"
#include "UiViewport.hpp"
#include "UiInspector.hpp"
#include "UiBottomTabs.hpp"
#include "UiLabels.hpp"

namespace UI {

static void SetupDefaultDockLayout(ImGuiID dockspace_id, const ImVec2& size) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, size);

    ImGuiID dockBottom = 0;
    ImGuiID dockMain = dockspace_id;
    ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.22f, &dockBottom, &dockMain);

    ImGuiID dockLeft = 0;
    ImGuiID dockCenter = 0;
    ImGuiID dockRight = 0;
    ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.22f, &dockLeft, &dockCenter);
    ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Right, 0.24f, &dockRight, &dockCenter);

    ImGui::DockBuilderDockWindow(kProjectWindowTitle, dockLeft);
    ImGui::DockBuilderDockWindow(kViewportWindowTitle, dockCenter);
    ImGui::DockBuilderDockWindow(kInspectorWindowTitle, dockRight);
    ImGui::DockBuilderDockWindow(kBottomWindowTitle, dockBottom);
    ImGui::DockBuilderFinish(dockspace_id);
}

static void DockSpace() {
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    static bool dock_layout_initialized = false;
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("DockSpaceRoot", nullptr, window_flags);
    ImGui::PopStyleVar(2);

    ImGuiID dockspace_id = ImGui::GetID("PuzzleDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0, 0), dockspace_flags);
    if (!dock_layout_initialized) {
        const ImVec2 viewport_size = ImVec2(viewport->WorkSize.x, viewport->WorkSize.y);
        SetupDefaultDockLayout(dockspace_id, viewport_size);
        dock_layout_initialized = true;
    }

    ImGui::End();
}

void DrawUi(App::EditorAppState& s, SDL_Renderer* renderer) {
    DockSpace();

    DrawProjectPanel(s);
    DrawViewportPanel(s, renderer);
    DrawInspectorPanel(s);
    DrawBottomTabs(s);
}

}  // namespace UI
