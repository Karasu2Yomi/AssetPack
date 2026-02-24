#pragma once
#include <vector>
#include <cstdint>
#include "domain/TileMap.hpp"

namespace Pipeline {

    struct CellDiff {
        int layerIndex;
        int x;
        int y;
        int32_t a; // json
        int32_t b; // bin
    };

    std::vector<CellDiff> DiffMaps(const Domain::TileMap& a, const Domain::TileMap& b, size_t maxCount = 2000);

}