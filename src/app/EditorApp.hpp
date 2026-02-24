// EditorApp.h
#pragma once
#include "EditorAppState.hpp"

namespace App {

    class EditorApp {
    public:
        EditorApp();
        void InitDefaultProject(EditorAppState& s); // create layers, default tileset etc.
        void Tick(EditorAppState& s);               // state update if needed (future)
    };

}