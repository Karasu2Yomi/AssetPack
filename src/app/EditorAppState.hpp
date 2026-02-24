#pragma once
#include <string>
#include <vector>
#include "domain/TileMap.hpp"
#include "pipeline/Validator.hpp"
#include "pipeline/Diff.hpp"

namespace App {

    struct EditorAppState {
        std::string projectRoot = "Project";

        std::string currentJsonPath = "Project/maps/stage01.json";
        std::string currentBinPath  = "Project/build/stage01.bin";

        Domain::TileMap jsonMap; // source
        Domain::TileMap binMap;  // imported result

        bool hasBinLoaded = false;
        bool splitView = true;

        // selection / brush
        int activeLayer = 0;
        int selectedTileId = 1;
        bool showGrid = true;

        // results
        std::vector<Pipeline::Issue> issues;
        std::vector<Pipeline::CellDiff> diffs;

        // console log
        std::vector<std::string> console;
    };

} // namespace app
