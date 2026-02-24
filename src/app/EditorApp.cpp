#include "EditorApp.hpp"
#include "Logger.hpp"

namespace App {

    EditorApp::EditorApp() {}

    void EditorApp::InitDefaultProject(EditorAppState& s) {
        s.jsonMap.name = "stage01";
        s.jsonMap.width = 64;
        s.jsonMap.height = 36;
        s.jsonMap.tileSize = 16;
        s.jsonMap.tileset.imagePath = "tilesets/basic.png";
        s.jsonMap.tileset.count = 256;
        s.jsonMap.layers = {
        {"Ground", Domain::LayerType::Tile, true, {}},
        {"Collision", Domain::LayerType::Collision, true, {}},
      };
        s.jsonMap.EnsureLayerDataSizes();
        Log(s, "Initialized default project state.");
    }

    void EditorApp::Tick(EditorAppState& /*s*/) {
        // For future: autosave, background validation triggers, etc.
    }

}