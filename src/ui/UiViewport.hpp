#pragma once
#include "app/EditorAppState.hpp"
#include "SDL3/SDL_render.h"

namespace UI {
    void DrawViewportPanel(App::EditorAppState& s, SDL_Renderer* renderer);
}