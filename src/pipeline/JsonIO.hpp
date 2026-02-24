#pragma once
#include <string>
#include "domain/TileMap.hpp"
#include "IO.hpp"

namespace Pipeline {



    IoError SaveJson(const std::string& path, const Domain::TileMap& map);
    IoError LoadJson(const std::string& path, Domain::TileMap& out);

}

