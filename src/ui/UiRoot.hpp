#pragma once
#include "app/EditorAppState.hpp"
#include "SDL3/SDL_render.h"

namespace UI {
    void DrawUi(App::EditorAppState& s, SDL_Renderer* renderer);
}
