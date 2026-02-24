#include "Validator.hpp"
#include <sstream>

namespace Pipeline {

    std::vector<Issue> Validate(const Domain::TileMap& map) {
        std::vector<Issue> out;
        const auto n = map.CellCount();

        if (map.layers.empty()) {
            out.push_back({Severity::Error, "No layers.", -1, -1, -1});
            return out;
        }

        for (int li = 0; li < (int)map.layers.size(); ++li) {
            const auto& layer = map.layers[li];
            if (layer.data.size() != n) {
                out.push_back({Severity::Error, "Layer data size mismatch.", -1, -1, li});
                continue;
            }
            for (uint32_t idx = 0; idx < n; ++idx) {
                const int32_t id = layer.data[idx];
                if (id < 0) continue; // allow -1 as empty if you want
                if ((uint32_t)id >= map.tileset.count) {
                    int x = (int)(idx % map.width);
                    int y = (int)(idx / map.width);
                    std::ostringstream oss;
                    oss << "tileId out of range: " << id << " (count=" << map.tileset.count << ")";
                    out.push_back({Severity::Error, oss.str(), x, y, li});
                    // only report a few for MVP
                    if (out.size() > 50) return out;
                }
            }
        }

        if (map.tileset.tileWidth != map.tileSize || map.tileset.tileHeight != map.tileSize) {
            out.push_back({Severity::Warning, "tileset tile size != map tileSize", -1, -1, -1});
        }
        if (map.tileset.imagePath.empty()) {
            out.push_back({Severity::Warning, "tileset.imagePath is empty", -1, -1, -1});
        }

        return out;
    }

}