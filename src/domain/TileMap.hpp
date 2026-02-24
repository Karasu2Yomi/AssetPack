#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Domain {

    enum class LayerType : uint8_t {
        Tile = 0,
        Collision = 1,
      };

    struct Tileset {
        std::string imagePath;  // relative path (e.g., "tilesets/basic.png")
        uint32_t tileWidth = 16;
        uint32_t tileHeight = 16;
        uint32_t columns = 16;
        uint32_t count = 256;
    };

    struct Layer {
        std::string name;
        LayerType type = LayerType::Tile;
        bool visible = true;
        // Row-major: data[y*width + x]
        std::vector<int32_t> data;
    };

    struct TileMap {
        uint32_t version = 1;

        std::string name = "stage_01";
        uint32_t width = 64;
        uint32_t height = 36;
        uint32_t tileSize = 16;

        Tileset tileset;
        std::vector<Layer> layers;

        void EnsureLayerDataSizes();
        size_t CellCount() const { return static_cast<size_t>(width) * static_cast<size_t>(height); }
    };

}
