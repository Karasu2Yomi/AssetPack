#include "UiBottomTabs.hpp"
#include "UiLabels.hpp"

#include <algorithm>
#include <imgui.h>

#include "app/Logger.hpp"
#include "assetpack_core/PuzzleProjectStore.hpp"

namespace {

using namespace App;

static const char* SeverityText(const AssetPackCore::IssueSeverity s) {
    return s == AssetPackCore::IssueSeverity::Error ? "エラー" : "警告";
}

}  // namespace

namespace UI {

void DrawBottomTabs(App::EditorAppState& s) {
    ImGui::Begin(kBottomWindowTitle,
                 nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse);

    if (ImGui::BeginTabBar("BottomTabs")) {
        if (ImGui::BeginTabItem("問題")) {
            for (const auto& it : s.issues) {
                const std::string loc = it.file +
                    (it.line > 0 ? "（" + std::to_string(it.line) + "行目）" : "");
                const std::string field =
                    it.field.empty() ? std::string{} : std::string("（") + FieldLabel(it.field) + "）";
                ImGui::TextWrapped("[%s] %s%s %s", SeverityText(it.severity), loc.c_str(),
                            field.c_str(), it.message.c_str());
            }
            if (s.issues.empty()) {
                ImGui::TextUnformatted("問題はありません。");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("ログ")) {
            if (ImGui::Button("ログを消去##Clear")) {
                s.console.clear();
            }
            ImGui::Separator();
            for (const auto& line : s.console) {
                ImGui::TextWrapped("%s", line.c_str());
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (s.promptSaveBeforeClose) {
        ImGui::OpenPopup("終了の確認###close_confirm");
    }
    if (ImGui::BeginPopupModal("終了の確認###close_confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("保存されていない変更があります。");
        ImGui::TextUnformatted("閉じる前にどうしますか？");
        if (ImGui::Button("保存して終了")) {
            if (AssetPackCore::PuzzleProjectStore::SaveAll(s.project, s.issues)) {
                s.promptSaveBeforeClose = false;
                s.shouldExit = true;
                s.requestClose = false;
                ImGui::CloseCurrentPopup();
            } else {
                App::Log(s, "保存に失敗したため終了を中断します。");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("破棄して終了")) {
            s.promptSaveBeforeClose = false;
            s.shouldExit = true;
            s.requestClose = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("キャンセル")) {
            s.promptSaveBeforeClose = false;
            s.requestClose = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

}  // namespace UI
