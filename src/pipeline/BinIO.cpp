#include "BinIO.hpp"
#include <fstream>
#include <cstring>

namespace Pipeline {

    static void WriteU32(std::ofstream& ofs, uint32_t v) { ofs.write(reinterpret_cast<const char*>(&v), sizeof(v)); }
    static void ReadU32(std::ifstream& ifs, uint32_t& v) { ifs.read(reinterpret_cast<char*>(&v), sizeof(v)); }

    IoError SaveBin(const std::string& path, const Domain::TileMap& map) {
        try {
            std::ofstream ofs(path, std::ios::binary);
            if (!ofs) return {false, "Failed to open for write: " + path};

            BinHeader h;
            h.version = map.version;
            h.width = map.width;
            h.height = map.height;
            h.tileSize = map.tileSize;
            h.layerCount = static_cast<uint32_t>(map.layers.size());

            ofs.write(reinterpret_cast<const char*>(&h), sizeof(h));

            // Store layer types then raw tile ids
            for (const auto& layer : map.layers) {
                uint32_t type = static_cast<uint32_t>(layer.type);
                WriteU32(ofs, type);
            }

            const auto n = map.CellCount();
            for (const auto& layer : map.layers) {
                if (layer.data.size() != n) return {false, "Layer data size mismatch."};
                ofs.write(reinterpret_cast<const char*>(layer.data.data()), static_cast<std::streamsize>(n * sizeof(int32_t)));
            }

            return {true, ""};
        } catch (const std::exception& e) {
            return {false, std::string("SaveBin exception: ") + e.what()};
        }
    }

    IoError LoadBin(const std::string& path, Domain::TileMap& out) {
        try {
            std::ifstream ifs(path, std::ios::binary);
            if (!ifs) return {false, "Failed to open for read: " + path};

            BinHeader h{};
            ifs.read(reinterpret_cast<char*>(&h), sizeof(h));
            if (!ifs) return {false, "Failed to read header."};
            if (h.magic != 0x50414D54) return {false, "Invalid magic (expected TMAP)."};
            if (h.width == 0 || h.height == 0) return {false, "Invalid map size."};

            out.version = h.version;
            out.width = h.width;
            out.height = h.height;
            out.tileSize = h.tileSize;
            out.layers.clear();
            out.layers.resize(h.layerCount);

            // read layer types
            for (uint32_t i = 0; i < h.layerCount; ++i) {
                uint32_t t = 0; ReadU32(ifs, t);
                out.layers[i].type = (t == 1) ? Domain::LayerType::Collision : Domain::LayerType::Tile;
                out.layers[i].name = (out.layers[i].type == Domain::LayerType::Collision) ? "Collision" : ("Layer" + std::to_string(i));
                out.layers[i].visible = true;
            }

            const auto n = out.CellCount();
            for (auto& layer : out.layers) layer.data.resize(n);

            for (uint32_t i = 0; i < h.layerCount; ++i) {
                ifs.read(reinterpret_cast<char*>(out.layers[i].data.data()), static_cast<std::streamsize>(n * sizeof(int32_t)));
                if (!ifs) return {false, "Failed to read layer data."};
            }

            return {true, ""};
        } catch (const std::exception& e) {
            return {false, std::string("LoadBin exception: ") + e.what()};
        }
    }

}