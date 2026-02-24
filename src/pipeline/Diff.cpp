#include "Diff.hpp"

namespace Pipeline {

    std::vector<CellDiff> DiffMaps(const Domain::TileMap& a, const Domain::TileMap& b, size_t maxCount) {
        std::vector<CellDiff> out;

        if (a.width != b.width || a.height != b.height || a.layers.size() != b.layers.size()) {
            out.push_back({-1, -1, -1, 0, 0});
            return out;
        }

        const auto n = a.CellCount();
        for (int li = 0; li < (int)a.layers.size(); ++li) {
            if (a.layers[li].data.size() != n || b.layers[li].data.size() != n) continue;
            for (uint32_t idx = 0; idx < n; ++idx) {
                const int32_t va = a.layers[li].data[idx];
                const int32_t vb = b.layers[li].data[idx];
                if (va != vb) {
                    int x = (int)(idx % a.width);
                    int y = (int)(idx / a.width);
                    out.push_back({li, x, y, va, vb});
                    if (out.size() >= maxCount) return out;
                }
            }
        }
        return out;
    }

}