#include "PuzzleResolver.hpp"

#include <cmath>
#include <charconv>
#include <cstdlib>
#include <type_traits>

namespace {

template <typename T>
[[nodiscard]] bool Parse(const std::string& text, T& out) {
    if (text.empty()) {
        return false;
    }
    if constexpr (std::is_integral_v<T>) {
        auto ptr = text.data();
        auto end = text.data() + text.size();
        std::from_chars_result rc = std::from_chars(ptr, end, out);
        return rc.ec == std::errc{} && rc.ptr == end;
    } else {
        char* endPtr = nullptr;
        const double v = std::strtod(text.c_str(), &endPtr);
        if (endPtr == text.c_str() || *endPtr != '\0') {
            return false;
        }
        out = static_cast<T>(v);
        return true;
    }
}

} // namespace

namespace AssetPackCore {

ResolvedNodeView PuzzleResolver::Resolve(const NodePresetRow& preset,
                                        const MapNodeRow& node) {
    ResolvedNodeView out;
    out.instance_id = node.instance_id;
    out.source_preset_id = node.source_preset_id;

    out.node_type = node.node_type.overridden ? node.node_type.value
                                             : preset.node_type;
    out.texture_path = node.texture_path.overridden ? node.texture_path.value
                                                  : preset.texture_path;
    out.display_name = node.display_name.overridden ? node.display_name.value
                                                   : preset.display_name;

    const std::string widthSource =
        node.width_tiles.overridden ? node.width_tiles.value : preset.width_tiles;
    const std::string heightSource =
        node.height_tiles.overridden ? node.height_tiles.value : preset.height_tiles;
    const std::string incomingSource = node.max_incoming.overridden
                                          ? node.max_incoming.value
                                          : preset.max_incoming;
    const std::string outgoingSource = node.max_outgoing.overridden
                                          ? node.max_outgoing.value
                                          : preset.max_outgoing;
    const std::string outgoingLengthSource =
        node.max_outgoing_length.overridden ? node.max_outgoing_length.value
                                           : preset.max_outgoing_length;

    Parse(widthSource, out.width_tiles);
    Parse(heightSource, out.height_tiles);
    Parse(incomingSource, out.max_incoming);
    Parse(outgoingSource, out.max_outgoing);
    Parse(outgoingLengthSource, out.max_outgoing_length);

    const bool tileX = node.tile_x.overridden && !node.tile_x.value.empty();
    const bool tileY = node.tile_y.overridden && !node.tile_y.value.empty();
    if (tileX || tileY) {
        int x = 0;
        int y = 0;
        out.has_placement = Parse(node.tile_x.value, x) &&
                             Parse(node.tile_y.value, y);
        out.tile_x = x;
        out.tile_y = y;
    }
    return out;
}

} // namespace AssetPackCore
