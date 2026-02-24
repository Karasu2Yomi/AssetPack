#include "UiRoot.hpp"
#include <imgui.h>

#include "UiProject.hpp"
#include "UiViewport.hpp"
#include "UiInspector.hpp"
#include "UiBottomTabs.hpp"

namespace UI {

    static void DockSpace() {
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
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

        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0, 0), dockspace_flags);

        ImGui::End();
    }

    void DrawUi(App::EditorAppState& s, SDL_Renderer* renderer) {
        DockSpace();

        DrawProjectPanel(s);
        DrawViewportPanel(s, renderer);
        DrawInspectorPanel(s);
        DrawBottomTabs(s);
    }

} // namespace ui
