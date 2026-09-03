//#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cstdio>
#include <filesystem>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include "app/EditorApp.hpp"
#include "app/Logger.hpp"
#include "ui/UiRoot.hpp"

static void Fail(const char* message, int rc = 0, SDL_Window* window = nullptr) {
    const char* err = SDL_GetError();
    // Keep third-party diagnostics in stderr, not in the localized dialog.
    std::fprintf(stderr, "[エラー] %s\n終了コード: %d\n詳細: %s\n", message, rc,
                 (err && *err) ? err : "詳細情報はありません。");
    const SDL_MessageBoxButtonData button = {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "閉じる"};
    SDL_MessageBoxData dialog{};
    dialog.flags = SDL_MESSAGEBOX_ERROR;
    dialog.window = window;
    dialog.title = "AssetPack - 起動エラー";
    dialog.message = message;
    dialog.numbuttons = 1;
    dialog.buttons = &button;
    SDL_ShowMessageBox(&dialog, nullptr);
}

static void SetJapaneseInterface() {
    static constexpr ImGuiLocEntry entries[] = {
        {ImGuiLocKey_VersionStr, "描画ライブラリのバージョン: " IMGUI_VERSION},
        {ImGuiLocKey_TableSizeOne, "列幅を内容に合わせる###SizeOne"},
        {ImGuiLocKey_TableSizeAllFit, "すべての列幅を内容に合わせる###SizeAll"},
        {ImGuiLocKey_TableSizeAllDefault, "すべての列幅を初期値に戻す###SizeAll"},
        {ImGuiLocKey_TableResetOrder, "列の順序を初期値に戻す###ResetOrder"},
        {ImGuiLocKey_WindowingMainMenuBar, "（メインメニュー）"},
        {ImGuiLocKey_WindowingPopup, "（ポップアップ）"},
        {ImGuiLocKey_WindowingUntitled, "（名称未設定）"},
        {ImGuiLocKey_OpenLink_s, "「%s」を開く"},
        {ImGuiLocKey_CopyLink, "リンクをコピー###CopyLink"},
        {ImGuiLocKey_DockingHideTabBar, "タブバーを隠す###HideTabBar"},
        {ImGuiLocKey_DockingHoldShiftToDock, "シフトキーを押しながら移動すると、パネルを結合できます。"},
        {ImGuiLocKey_DockingDragToUndockOrMoveNode, "ドラッグすると、パネル全体を移動したり切り離したりできます。"},
    };
    ImGui::LocalizeRegisterEntries(entries, IM_ARRAYSIZE(entries));
}

static bool LoadJapaneseFont() {
    ImGuiIO& io = ImGui::GetIO();
    bool loaded = false;

    const auto tryPath = [&](std::string_view path) -> bool {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec) || ec) return false;
        return io.Fonts->AddFontFromFileTTF(path.data(), 18.0f, nullptr,
                                           io.Fonts->GetGlyphRangesJapanese()) !=
               nullptr;
    };

    loaded = tryPath("C:/Windows/Fonts/msgothic.ttc");
    if (!loaded) loaded = tryPath("C:/Windows/Fonts/meiryo.ttc");
    if (!loaded) loaded = tryPath("C:/Windows/Fonts/YuGothM.ttc");
    if (!loaded) loaded = tryPath("C:/Windows/Fonts/msyh.ttc");
    if (!loaded) loaded = tryPath("C:/Windows/Fonts/msmincho.ttc");
    if (!loaded) {
        io.Fonts->AddFontDefault();
    }
    return loaded;
}

int main(int, char**) {
    ::ShowWindow(::GetConsoleWindow(), SW_HIDE);

    SDL_SetMainReady();
    int rc = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    if (!rc) {
        Fail("映像機能を初期化できませんでした。画面設定やドライバーを確認してください。", rc);
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("AssetPack - マップエディター", 1400, 900, SDL_WINDOW_RESIZABLE);
    if (!window) {
        Fail("編集ウィンドウを作成できませんでした。");
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        Fail("描画機能を初期化できませんでした。画面設定やドライバーを確認してください。", 0, window);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    SetJapaneseInterface();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForSDLRenderer(window, renderer)) {
        Fail("画面の入力処理を初期化できませんでした。", 0, window);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!ImGui_ImplSDLRenderer3_Init(renderer)) {
        Fail("編集画面の描画処理を初期化できませんでした。", 0, window);
        ImGui_ImplSDL3_Shutdown();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    App::EditorAppState state;
    App::EditorApp app;
    app.InitDefaultProject(state);
    App::Log(state, "AssetPack マップエディターを起動しました。");
    if (!LoadJapaneseFont()) {
        App::Log(state, "警告: 日本語フォントを読み込めませんでした。デフォルトフォントを使用します。");
    }

    bool running = true;
    while (running) {
        SDL_Event e;
        bool saveShortcut = false;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                state.requestClose = true;
            }
            if (e.type == SDL_EVENT_KEY_DOWN &&
                (e.key.mod & SDL_KMOD_CTRL) &&
                e.key.scancode == SDL_SCANCODE_S) {
                saveShortcut = true;
            }
        }

        if (saveShortcut && state.projectLoaded) {
            if (app.SaveAll(state)) {
                App::Log(state, "保存しました。");
            } else {
                App::Log(state, "保存に失敗しました。");
            }
        }

        if (state.requestClose) {
            if (!state.projectLoaded || !state.project.HasDirty()) {
                state.shouldExit = true;
                state.requestClose = false;
            } else {
                state.promptSaveBeforeClose = true;
            }
        }

        if (state.shouldExit) {
            break;
        }

        app.Tick(state);
        std::string title = "AssetPack - マップエディター";
        if (state.projectLoaded) {
            title += " - " + state.project.dataRoot.filename().generic_string();
            if (state.project.HasDirty()) {
                title = "*" + title;
            }
        }
        SDL_SetWindowTitle(window, title.c_str());

        ImGui_ImplSDL3_NewFrame();
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui::NewFrame();

        UI::DrawUi(state, renderer);

        ImGui::Render();

        SDL_SetRenderDrawColor(renderer, 20, 20, 23, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
