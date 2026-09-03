//#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cstdio>
#include <filesystem>
#include <string_view>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include "app/EditorApp.hpp"
#include "app/Logger.hpp"
#include "ui/UiRoot.hpp"

static void Fail(const char* where, int rc = 0) {
    const char* err = SDL_GetError();
    char buf[512];
    std::snprintf(
        buf, sizeof(buf),
        "%s\nrc=%d\nSDL_GetError()=%s", where, rc,
        (err && *err) ? err : "(empty)");
    std::fprintf(stderr, "[FATAL] %s\n", buf);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "AssetPack Fatal", buf, nullptr);
}

static bool LoadJapaneseFont() {
    ImGuiIO& io = ImGui::GetIO();
    bool loaded = false;

    const auto tryPath = [&](std::string_view path) -> bool {
        return io.Fonts->AddFontFromFileTTF(path.data(), 18.0f, nullptr,
                                           io.Fonts->GetGlyphRangesJapanese()) !=
               nullptr;
    };

    loaded = tryPath("C:/Windows/Fonts/msgothic.ttc");
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
        Fail("SDL_Init", rc);
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("AssetPack", 1400, 900, SDL_WINDOW_RESIZABLE);
    if (!window) {
        Fail("SDL_CreateWindow");
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        Fail("SDL_CreateRenderer");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForSDLRenderer(window, renderer)) {
        std::fprintf(stderr, "[FATAL] ImGui_ImplSDL3_InitForSDLRenderer failed\n");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "AssetPack Fatal",
                                "ImGui_ImplSDL3_InitForSDLRenderer failed",
                                window);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!ImGui_ImplSDLRenderer3_Init(renderer)) {
        std::fprintf(stderr, "[FATAL] ImGui_ImplSDLRenderer3_Init failed\n");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "AssetPack Fatal",
                                "ImGui_ImplSDLRenderer3_Init failed", window);
        ImGui_ImplSDL3_Shutdown();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    App::EditorAppState state;
    App::EditorApp app;
    app.InitDefaultProject(state);
    App::Log(state, "AssetPack Puzzle Editor 起動");
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
        std::string title = "AssetPack";
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
