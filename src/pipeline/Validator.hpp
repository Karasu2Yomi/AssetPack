#pragma once
#include <string>
#include <vector>
#include "domain/TileMap.hpp"

namespace Pipeline {

    enum class Severity { Error, Warning };

    struct Issue {
        Severity severity;
        std::string message;
        int x = -1;
        int y = -1;
        int layerIndex = -1;
    };

    std::vector<Issue> Validate(const Domain::TileMap& map);

}