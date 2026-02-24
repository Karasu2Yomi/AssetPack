#include "UiBottomTabs.hpp"
#include <imgui.h>

#include "pipeline/Validator.hpp"
#include "pipeline/JsonIO.hpp"
#include "pipeline/BinIO.hpp"
#include "pipeline/Diff.hpp"
#include "app/Logger.hpp"
#include "util/PathUtil.hpp"

namespace UI {

    static const char* Sev(Pipeline::Severity s) { return s == Pipeline::Severity::Error ? "Error" : "Warn"; }

    void DrawBottomTabs(App::EditorAppState& s) {
        ImGui::Begin("Bottom");

        if (ImGui::BeginTabBar("BottomTabs")) {

            if (ImGui::BeginTabItem("Console")) {
                if (ImGui::Button("Clear")) s.console.clear();
                ImGui::Separator();
                for (const auto& line : s.console) ImGui::TextUnformatted(line.c_str());
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Validate")) {
                if (ImGui::Button("Run Validate")) {
                    s.issues = Pipeline::Validate(s.jsonMap);
                    App::Log(s, "Validate executed. issues=" + std::to_string(s.issues.size()));
                }
                ImGui::Separator();
                for (const auto& it : s.issues) {
                    ImGui::Text("[%s] (L%d x=%d y=%d) %s", Sev(it.severity), it.layerIndex, it.x, it.y, it.message.c_str());
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Export/Import")) {
                if (ImGui::Button("Save JSON")) {
                    auto r = Pipeline::SaveJson(s.currentJsonPath, s.jsonMap);
                    App::Log(s, r.ok ? "Saved JSON." : ("Save JSON failed: " + r.message));
                }
                ImGui::SameLine();
                if (ImGui::Button("Load JSON")) {
                    auto base = Util::GetExeDir() / "Project";
                    auto r = Pipeline::LoadJson((base / s.currentJsonPath).generic_string(), s.jsonMap);
                    App::Log(s, r.ok ? "Loaded JSON." : ("Load JSON failed: " + r.message));
                }

                ImGui::Separator();
                if (ImGui::Button("Export BIN")) {
                    s.issues = Pipeline::Validate(s.jsonMap);
                    bool hasError = false;
                    for (auto& it : s.issues) if (it.severity == Pipeline::Severity::Error) { hasError = true; break; }
                    if (hasError) {
                        App::Log(s, "Export canceled: validation errors.");
                    } else {
                        auto r = Pipeline::SaveBin(s.currentBinPath, s.jsonMap);
                        App::Log(s, r.ok ? "Exported BIN." : ("Export BIN failed: " + r.message));
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Import BIN")) {
                    auto r = Pipeline::LoadBin(s.currentBinPath, s.binMap);
                    s.hasBinLoaded = r.ok;
                    App::Log(s, r.ok ? "Imported BIN." : ("Import BIN failed: " + r.message));
                }

                ImGui::Separator();
                if (ImGui::Button("Round-trip Diff (JSON vs BIN)")) {
                    if (!s.hasBinLoaded) {
                        App::Log(s, "Diff: BIN not loaded.");
                    } else {
                        s.diffs = Pipeline::DiffMaps(s.jsonMap, s.binMap, 2000);
                        App::Log(s, "Diff executed. diffs=" + std::to_string(s.diffs.size()));
                    }
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Diff")) {
                if (!s.hasBinLoaded) {
                    ImGui::TextUnformatted("BIN not loaded. Use Import BIN first.");
                } else {
                    ImGui::Text("Diff count: %d", (int)s.diffs.size());
                    ImGui::Separator();
                    for (const auto& d : s.diffs) {
                        ImGui::Text("L%d (%d,%d) json=%d bin=%d", d.layerIndex, d.x, d.y, d.a, d.b);
                    }
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

}