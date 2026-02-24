#include "UiProject.hpp"
#include <imgui.h>
#include "platform/WinFileDialog.hpp"
#include <misc/cpp/imgui_stdlib.h>
#include "util/PathUtil.hpp"

namespace UI {

    void DrawProjectPanel(App::EditorAppState& s) {
        ImGui::Begin("Project");

        ImGui::Text("Root: %s", s.projectRoot.c_str());
        ImGui::Separator();

        ImGui::Text("Tileset");
        ImGui::InputText("image", &s.jsonMap.tileset.imagePath);
        ImGui::SameLine();
        if (ImGui::Button("...##tileset")) {
            #ifdef _WIN32
            std::string picked;
            if (Platform::OpenFileDialog(picked, L"Select Tileset PNG", L"PNG Image (*.png)", L"*.png")) {
                //s.jsonMap.tileset.imagePath = picked;
                auto base = Util::GetExeDir() / "Project";              // exe直下の Project
                auto rel = Util::TryMakeRelativeIfUnderBase(picked, base);
                if (rel) {
                    s.jsonMap.tileset.imagePath = rel->generic_string();
                } else {
                    auto res = Util::CopyIntoBaseAndMakeRelative(picked, base, "tileset", false);
                    if (res.ok) s.jsonMap.tileset.imagePath = res.relativeDst.generic_string();
                }
            }
            #endif
        }

        ImGui::Separator();
        ImGui::Text("Maps (JSON)");
        ImGui::InputText("json path", &s.currentJsonPath);
        ImGui::SameLine();
        if (ImGui::Button("...##json")) {
            #ifdef _WIN32
            std::string picked;
            if (Platform::OpenFileDialog(picked, L"Select Map JSON", L"JSON (*.json)", L"*.json")) {
                //s.currentJsonPath = picked;
                auto base = Util::GetExeDir() / "Project";              // exe直下の Project
                auto rel = Util::TryMakeRelativeIfUnderBase(picked, base);
                if (rel) {
                    s.currentJsonPath = rel->generic_string();
                } else {
                    auto res = Util::CopyIntoBaseAndMakeRelative(picked, base, "maps", false);
                    if (res.ok) s.currentJsonPath = res.relativeDst.generic_string();
                }
            }
            #endif
        }

        ImGui::Text("Build (BIN)");
        ImGui::InputText("bin path", &s.currentBinPath);
        ImGui::SameLine();
        if (ImGui::Button("...##bin")) {
            #ifdef _WIN32
            std::string picked;
            if (Platform::OpenFolderDialog(picked, L"Select Output Folder")) {
                s.currentBinPath = picked + "\\stage01.bin"; // 存成目錄

            }
            #endif
        }

        ImGui::End();
    }

}