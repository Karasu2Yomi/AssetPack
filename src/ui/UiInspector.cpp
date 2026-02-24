#include "UiInspector.hpp"
#include <imgui.h>

namespace UI {

    void DrawInspectorPanel(App::EditorAppState& s) {
        ImGui::Begin("Inspector");

        ImGui::Checkbox("Show Grid", &s.showGrid);
        ImGui::Checkbox("Split View (JSON vs BIN)", &s.splitView);

        ImGui::Separator();
        ImGui::Text("Active Layer");
        for (int i = 0; i < (int)s.jsonMap.layers.size(); ++i) {
            ImGui::RadioButton(s.jsonMap.layers[i].name.c_str(), &s.activeLayer, i);
        }

        ImGui::Separator();
        ImGui::Text("Brush");
        ImGui::SliderInt("tileId", &s.selectedTileId, 0, (int)s.jsonMap.tileset.count - 1);

        ImGui::End();
    }

}