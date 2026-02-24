#include "Domain/TileMap.hpp"

namespace Domain {

    void TileMap::EnsureLayerDataSizes() {
        const auto n = CellCount();
        for (auto& layer : layers) {
            if (layer.data.size() != n) {
                layer.data.resize(n, 0);
            }
        }
    }

} // namespace domain
