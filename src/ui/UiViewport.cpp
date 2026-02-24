#include "UiViewport.hpp"
#include "TilesetTextureCache.hpp"
#include <imgui.h>
#include "SDL3/SDL_render.h"

namespace UI {

    static void DrawBrushTileOverlay(
        SDL_Texture* tilesetTex, int texW, int texH,
        const Domain::Tileset& ts,
        int selectedTileId) {

        if (!tilesetTex) return;
        if (selectedTileId <= 0) return;                 // 0は空扱い
        const int tileIndex = selectedTileId - 1;        // 0始まりへ

        if (tileIndex < 0 || tileIndex >= (int)ts.count) return;
        if (ts.columns == 0 || ts.tileWidth == 0 || ts.tileHeight == 0) return;
        if (texW <= 0 || texH <= 0) return;

        const int col = tileIndex % (int)ts.columns;
        const int row = tileIndex / (int)ts.columns;

        // UV（0..1）
        const float u0 = (float)(col * (int)ts.tileWidth)  / (float)texW;
        const float v0 = (float)(row * (int)ts.tileHeight) / (float)texH;
        const float u1 = (float)((col + 1) * (int)ts.tileWidth)  / (float)texW;
        const float v1 = (float)((row + 1) * (int)ts.tileHeight) / (float)texH;

        // 置く場所：Viewportウィンドウの右上（オーバーレイ）
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 wpos  = ImGui::GetWindowPos();
        const ImVec2 cmin  = ImGui::GetWindowContentRegionMin();
        const ImVec2 cmax  = ImGui::GetWindowContentRegionMax();

        const float pad = 10.0f;
        const float preview = 96.0f; // 表示サイズ（適当に固定）
        const ImVec2 p0(wpos.x + cmax.x - preview - pad, wpos.y + cmin.y + pad);
        const ImVec2 p1(p0.x + preview, p0.y + preview);

        // 背景枠（任意）
        dl->AddRectFilled(ImVec2(p0.x - 6, p0.y - 24), ImVec2(p1.x + 6, p1.y + 6),
                          IM_COL32(0, 0, 0, 160), 6.0f);
        dl->AddRect(ImVec2(p0.x - 6, p0.y - 24), ImVec2(p1.x + 6, p1.y + 6),
                    IM_COL32(255, 255, 255, 80), 6.0f);

        // ラベル
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Brush tileId=%d (r%d,c%d)", selectedTileId, row, col);
        dl->AddText(ImVec2(p0.x - 2, p0.y - 20), IM_COL32(255, 255, 255, 200), buf);

        // タイル1枚をUV指定で描画
        dl->AddImage((ImTextureID)tilesetTex, p0, p1, ImVec2(u0, v0), ImVec2(u1, v1));
    }

    static void DrawGrid(const Domain::TileMap& map, int layerIndex, bool showGrid, const char* tag) {
        ImGui::PushID(tag);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 avail = ImGui::GetContentRegionAvail();

        // Fit map into available area (simple scale)
        float cell = map.tileSize * 0.5f;
        float wpx = map.width * cell;
        float hpx = map.height * cell;

        ImVec2 canvasSize = ImVec2((wpx < avail.x ? wpx : avail.x), (hpx < avail.y ? hpx : avail.y));
        ImGui::InvisibleButton("canvas", canvasSize);

        // background
        dl->AddRectFilled(p0, ImVec2(p0.x + canvasSize.x, p0.y + canvasSize.y), IM_COL32(30, 30, 30, 255));

        if (layerIndex < 0 || layerIndex >= (int)map.layers.size()) return;
        const auto& layer = map.layers[layerIndex];
        const auto n = map.CellCount();
        if (layer.data.size() != n) return;

        // draw cells as grayscale blocks based on tileId
        for (uint32_t y = 0; y < map.height; ++y) {
            for (uint32_t x = 0; x < map.width; ++x) {
                uint32_t idx = y * map.width + x;
                int v = layer.data[idx];
                if (v == 0) continue;
                int c = (v * 17) & 255;
                ImU32 col = IM_COL32(c, c, c, 255);
                ImVec2 a(p0.x + x * cell, p0.y + y * cell);
                ImVec2 b(a.x + cell, a.y + cell);
                dl->AddRectFilled(a, b, col);
                if (showGrid) dl->AddRect(a, b, IM_COL32(70, 70, 70, 120));
            }
        }
        ImGui::PopID();
    }

    void DrawViewportPanel(App::EditorAppState& s, SDL_Renderer* renderer) {
        ImGui::Begin("Viewport");

        if (s.splitView) {
            ImGui::TextUnformatted("JSON Preview");
            DrawGrid(s.jsonMap, s.activeLayer, s.showGrid, "json");

            ImGui::Separator();
            ImGui::TextUnformatted("BIN Preview");
            if (s.hasBinLoaded) {
                DrawGrid(s.binMap, s.activeLayer, s.showGrid, "bin");
            } else {
                ImGui::TextUnformatted("BIN not loaded.");
            }
        } else {
            ImGui::TextUnformatted("Preview (JSON)");
            DrawGrid(s.jsonMap, s.activeLayer, s.showGrid, "json");
        }

        // tileset texture を取得（あなたの既存キャッシュを流用）
        // SDL_Texture* tex = nullptr;
        // int texW = 0, texH = 0;
        // if (GetOrLoadTilesetTexture(renderer, s.projectRoot, s.jsonMap.tileset.imagePath, &tex, &texW, &texH)) {
        //     DrawBrushTileOverlay(tex, texW, texH, s.jsonMap.tileset, s.selectedTileId);
        // }

        std::string err;
        SDL_Texture* tex = nullptr;
        UI::TilesetTexture g_tilesetTex;
        int texW = 0, texH = 0;

        if (UI::GetOrLoadTilesetTexture(renderer, s.projectRoot, s.jsonMap.tileset.imagePath, g_tilesetTex, &err)) {
            // tileset全体を表示（簡易）
            if (ImGui::CollapsingHeader("Tileset Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                float maxW = (avail.x > 1.0f ? avail.x : 400.0f);
                float scale = 1.0f;
                if (g_tilesetTex.w > 0 && (float)g_tilesetTex.w > maxW) {
                    scale = maxW / (float)g_tilesetTex.w;
                }
                ImVec2 size((float)g_tilesetTex.w * scale, (float)g_tilesetTex.h * scale);
                ImGui::Image((ImTextureID)g_tilesetTex.tex, size);
            }

            // Brush tile をViewport右上に出したいなら、ここで前に出した DrawBrushTileOverlay を呼ぶ
            DrawBrushTileOverlay(tex, texW, texH, s.jsonMap.tileset, s.selectedTileId);

        } else {
            ImGui::Text("Tileset load failed: %s", err.c_str());
        }

        ImGui::End();
    }

}
