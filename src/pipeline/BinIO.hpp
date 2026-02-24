#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "domain/TileMap.hpp"
#include "IO.hpp"

namespace Pipeline {

    struct BinHeader {
        uint32_t magic = 0x50414D54; // 'TMAP' little-endian
        uint32_t version = 1;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t tileSize = 0;
        uint32_t layerCount = 0;
        uint32_t reserved = 0;
        uint32_t crc32 = 0; // optional (we keep 0 in this skeleton)
    };


    IoError SaveBin(const std::string& path, const Domain::TileMap& map);
    IoError LoadBin(const std::string& path, Domain::TileMap& out);

}