#include "UiViewport.hpp"

#include <algorithm>
#include <cmath>
#include <imgui.h>

#include "app/EditorApp.hpp"
#include "app/Logger.hpp"
#include "assetpack_core/PuzzleResolver.hpp"
#include "ui/TilesetTextureCache.hpp"
#include "ui/UiLabels.hpp"

namespace {

using namespace App;
using namespace AssetPackCore;

static UI::TextureCache g_textureCache;

static constexpr int kGridW = 80;
static constexpr int kGridH = 45;
static constexpr float kBaseTilePx = 16.0f;

static const NodePresetRow* FindPreset(const EditorAppState& s,
                                      const std::string& presetId) {
    for (const auto& preset : s.project.presets) {
        if (preset.preset_id == presetId) {
            return &preset;
        }
    }
    return nullptr;
}

static ResolvedNodeView ResolveNode(const EditorAppState& s,
                                   const MapNodeRow& node) {
    const NodePresetRow* preset = FindPreset(s, node.source_preset_id);
    if (preset) {
        return PuzzleResolver::Resolve(*preset, node);
    }
    return PuzzleResolver::Resolve(NodePresetRow{}, node);
}

static bool HasPlacement(const MapNodeRow& node) {
    return node.tile_x.overridden && node.tile_y.overridden &&
           !node.tile_x.value.empty() && !node.tile_y.value.empty();
}

static bool TryGetPlacement(const EditorAppState& s, const MapNodeRow& node,
                           int& px, int& py, int& w, int& h) {
    if (!HasPlacement(node)) {
        return false;
    }
    const auto resolved = ResolveNode(s, node);
    if (!resolved.has_placement) {
        return false;
    }

    px = resolved.tile_x;
    py = resolved.tile_y;
    w = resolved.width_tiles;
    h = resolved.height_tiles;

    if (w <= 0) {
        w = 1;
    }
    if (h <= 0) {
        h = 1;
    }
    return true;
}

static bool ScreenToTile(const ImVec2& origin, float tileSize, const ImVec2& cursor,
                        int& outX, int& outY) {
    const float lx = cursor.x - origin.x;
    const float ly = cursor.y - origin.y;
    if (lx < 0.0f || ly < 0.0f) return false;
    outX = static_cast<int>(std::floor(lx / tileSize));
    outY = static_cast<int>(std::floor(ly / tileSize));
    return outX >= 0 && outX < kGridW && outY >= 0 && outY < kGridH;
}

static ImU32 ColorByType(const std::string& nodeType, bool selected) {
    ImU32 c = IM_COL32(120, 120, 120, 180);
    if (nodeType == "root") {
        c = IM_COL32(40, 160, 120, 220);
    } else if (nodeType == "follow") {
        c = IM_COL32(80, 80, 230, 220);
    } else if (nodeType == "end") {
        c = IM_COL32(210, 70, 70, 220);
    } else if (nodeType == "dead") {
        c = IM_COL32(200, 180, 60, 220);
    }
    if (selected) c = IM_COL32(255, 255, 255, 255);
    return c;
}

static void DrawCanvasBg(EditorAppState& s, ImDrawList* drawList,
                        const ImVec2& origin, const ImVec2& size,
                        float tileSize) {
    drawList->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                            IM_COL32(18, 18, 26, 255));
    if (!s.canvas.showGrid) {
        return;
    }
    for (int x = 0; x <= kGridW; ++x) {
        const float px = origin.x + x * tileSize;
        drawList->AddLine(ImVec2(px, origin.y),
                          ImVec2(px, origin.y + size.y),
                          IM_COL32(48, 48, 70, 255));
    }
    for (int y = 0; y <= kGridH; ++y) {
        const float py = origin.y + y * tileSize;
        drawList->AddLine(ImVec2(origin.x, py),
                          ImVec2(origin.x + size.x, py),
                          IM_COL32(48, 48, 70, 255));
    }
}

static void DrawNodeTextLabel(ImDrawList* drawList, const ImVec2& p0,
                             const ImVec2& p1, bool selected,
                             const std::string& text) {
    const std::string show = text.empty() ? "名称未設定" : text;
    const ImVec2 labelSize = ImGui::CalcTextSize(show.c_str());
    const float x = p0.x + std::max(2.0f, (p1.x - p0.x - labelSize.x) * 0.5f);
    const float y = p0.y + std::max(2.0f, (p1.y - p0.y - labelSize.y) * 0.5f);
    const ImU32 textColor = selected ? IM_COL32(255, 255, 255, 255)
                                    : IM_COL32(230, 230, 230, 200);
    drawList->AddText(ImVec2(std::min(p1.x - labelSize.x - 2.0f, x),
                             std::min(p1.y - labelSize.y - 2.0f, y)),
                      textColor, show.c_str());
}

}  // namespace

namespace UI {

void DrawViewportPanel(App::EditorAppState& s, SDL_Renderer* renderer) {
    ImGui::Begin(kViewportWindowTitle,
                 nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse);

    MapDocument* map = s.SelectedMap();
    if (!map) {
        if (s.selectedLevelIndex < 0 || s.selectedLevelIndex >= (int)s.project.levels.size()) {
            ImGui::TextUnformatted("レベルを選択してください。");
        } else {
            ImGui::TextUnformatted("マップを取得できません。データフォルダを再読込してください。");
        }
        ImGui::TextUnformatted("マップキャンバスは 80x45 グリッドです。");
        ImGui::End();
        return;
    }

    if (map->state == MapDocumentState::NewDraft) {
        ImGui::TextUnformatted("新規マップ（未保存・編集可能）");
    } else if (map->state == MapDocumentState::MissingFileDraft) {
        ImGui::TextUnformatted("参照先ファイルなし（編集可能・保存時に作成）");
    }
    ImGui::Checkbox("グリッド", &s.canvas.showGrid);

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float availableW = std::max(240.0f, avail.x);
    const float availableH = std::max(180.0f, avail.y);
    const float baseTile = kBaseTilePx * s.canvas.zoom;
    const float fitTileX = availableW / kGridW;
    const float fitTileY = availableH / kGridH;
    const float tileSize = std::max(
        1.0f, std::min(baseTile, std::max(1.0f, std::min(fitTileX, fitTileY))));
    const ImVec2 nodeCanvasSize(kGridW * tileSize, kGridH * tileSize);

    const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    const ImVec2 origin(cursorPos.x + s.canvas.panX, cursorPos.y + s.canvas.panY);
    ImGui::InvisibleButton("canvas", nodeCanvasSize,
                           ImGuiButtonFlags_MouseButtonLeft |
                               ImGuiButtonFlags_MouseButtonMiddle);

    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 mousePos = ImGui::GetIO().MousePos;
    int hitTileX = -1;
    int hitTileY = -1;
    const bool cursorInGrid = hovered &&
                             ScreenToTile(origin, tileSize, mousePos, hitTileX, hitTileY);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    DrawCanvasBg(s, dl, origin, nodeCanvasSize, tileSize);

    bool anyOutOfBounds = false;
    for (int i = 0; i < (int)map->nodes.size(); ++i) {
        auto& node = map->nodes[i];
        int px = 0;
        int py = 0;
        int w = 1;
        int h = 1;
        const bool placed = TryGetPlacement(s, node, px, py, w, h);
        if (!placed) {
            continue;
        }

        const ResolvedNodeView resolved = ResolveNode(s, node);
        const auto resolvedType = resolved.node_type.empty() ? "follow" : resolved.node_type;
        const std::string texturePath = resolved.texture_path;

        const float x0 = origin.x + static_cast<float>(px) * tileSize;
        const float y0 = origin.y + static_cast<float>(py) * tileSize;
        const float x1 = x0 + static_cast<float>(w) * tileSize;
        const float y1 = y0 + static_cast<float>(h) * tileSize;
        const bool selected = (s.selectedTarget == SelectTarget::MapNode &&
                              s.selectedMapNodeIndex == i);

        const bool outOfBounds = (x0 < origin.x || y0 < origin.y ||
                                 x1 > origin.x + nodeCanvasSize.x ||
                                 y1 > origin.y + nodeCanvasSize.y);
        if (outOfBounds) {
            anyOutOfBounds = true;
            continue;
        }

        TextureData* texData = nullptr;
        std::string err;
        const bool hasTexture =
            !texturePath.empty() &&
            g_textureCache.GetOrLoad(renderer, texturePath,
                                     s.project.resourceRoot.generic_string(),
                                     texData, &err);
        if (hasTexture && texData && texData->texture) {
            SDL_SetTextureScaleMode(texData->texture, SDL_SCALEMODE_NEAREST);
            dl->AddImage(reinterpret_cast<ImTextureID>(texData->texture),
                         ImVec2(x0, y0), ImVec2(x1, y1));
        } else {
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1),
                              ColorByType(resolvedType, selected));
            DrawNodeTextLabel(dl, ImVec2(x0, y0), ImVec2(x1, y1), selected,
                              resolved.display_name.empty() ? node.instance_id
                                                           : resolved.display_name);
        }
        dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1),
                    selected ? IM_COL32(255, 255, 255, 255)
                             : IM_COL32(255, 255, 255, 80));
    }

    if (hovered && cursorInGrid) {
        const float hx = origin.x + hitTileX * tileSize;
        const float hy = origin.y + hitTileY * tileSize;
        dl->AddRect(ImVec2(hx, hy), ImVec2(hx + tileSize, hy + tileSize),
                    IM_COL32(255, 255, 255, 150));
    }

    if (s.canvas.showGrid && hovered && s.canvas.zoom >= 0.4f && cursorInGrid) {
        std::string msg = "ヒット: " + std::to_string(hitTileX) + "," +
                          std::to_string(hitTileY);
        dl->AddText(ImVec2(origin.x + 6.0f, origin.y + 6.0f),
                    IM_COL32(210, 210, 210, 200), msg.c_str());
    }

    const float wheel = ImGui::GetIO().MouseWheel;
    if (hovered && wheel != 0.0f) {
        s.canvas.zoom = std::clamp(s.canvas.zoom + wheel * 0.1f, 0.3f, 3.0f);
    }

    if (hovered && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        if (!s.panning) {
            s.panning = true;
            s.panAnchorX = mousePos.x;
            s.panAnchorY = mousePos.y;
            s.panBaseX = s.canvas.panX;
            s.panBaseY = s.canvas.panY;
        }
        if (s.panning) {
            s.canvas.panX = s.panBaseX + (mousePos.x - s.panAnchorX);
            s.canvas.panY = s.panBaseY + (mousePos.y - s.panAnchorY);
        }
    } else {
        s.panning = false;
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (s.isAddMode && !s.selectedPresetForAdd.empty()) {
            if (cursorInGrid && App::EditorApp::PlaceNode(s, hitTileX, hitTileY)) {
                App::Log(s, "ノードを追加しました。");
            }
        } else {
            int hitNodeIndex = -1;
            for (int i = (int)map->nodes.size() - 1; i >= 0; --i) {
                const auto& node = map->nodes[i];
                int px = 0;
                int py = 0;
                int w = 1;
                int h = 1;
                if (!TryGetPlacement(s, node, px, py, w, h)) {
                    continue;
                }
                if (hitTileX >= px && hitTileX < (px + w) &&
                    hitTileY >= py && hitTileY < (py + h)) {
                    hitNodeIndex = i;
                    break;
                }
            }

            s.selectedMapNodeIndex = hitNodeIndex;
            if (hitNodeIndex >= 0) {
                s.selectedTarget = SelectTarget::MapNode;
                s.draggingNode = cursorInGrid;
                s.draggingNodeIndex = hitNodeIndex;
            } else {
                s.selectedTarget = SelectTarget::None;
                s.draggingNode = false;
                s.draggingNodeIndex = -1;
            }
        }
    }

    if (s.draggingNode && s.draggingNodeIndex >= 0 &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left) && cursorInGrid &&
        s.draggingNodeIndex < (int)map->nodes.size()) {
        auto& node = map->nodes[s.draggingNodeIndex];
        node.tile_x.overridden = true;
        node.tile_y.overridden = true;
        node.tile_x.value = std::to_string(hitTileX);
        node.tile_y.value = std::to_string(hitTileY);
        map->dirty = true;
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && s.draggingNode) {
        s.draggingNode = false;
        s.draggingNodeIndex = -1;
    }

    ImGui::Dummy(nodeCanvasSize);
    if (s.selectedTarget == SelectTarget::MapNode &&
        s.selectedMapNodeIndex >= 0 &&
        s.selectedMapNodeIndex < (int)map->nodes.size()) {
        ImGui::Text("選択: %s", map->nodes[s.selectedMapNodeIndex].instance_id.c_str());
    } else {
        ImGui::Text("選択: なし");
    }
    if (s.isAddMode) {
        ImGui::Text("左クリック: マップへ追加");
    }
    if (anyOutOfBounds) {
        ImGui::TextUnformatted("警告: 一部のノードがキャンバス外にあります（保存可能）");
    }

    ImGui::End();
}

}  // namespace UI
