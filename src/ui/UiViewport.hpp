#pragma once

#include <SDL3/SDL_render.h>

#include "app/EditorAppState.hpp"

namespace UI {
    void DrawViewportPanel(App::EditorAppState& s, SDL_Renderer* renderer);
}
