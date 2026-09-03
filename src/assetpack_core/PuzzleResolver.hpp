#pragma once

#include "Model.hpp"

namespace AssetPackCore {

class PuzzleResolver {
public:
    static ResolvedNodeView Resolve(const NodePresetRow& preset,
                                    const MapNodeRow& node);
};

} // namespace AssetPackCore
