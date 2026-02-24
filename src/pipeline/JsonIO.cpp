#include "JsonIO.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

using nlohmann::json;

namespace {
    std::string ToString(Domain::LayerType t) {
        return (t == Domain::LayerType::Collision) ? "Collision" : "Tile";
    }
    Domain::LayerType ToLayerType(const std::string& s) {
        return (s == "Collision") ? Domain::LayerType::Collision : Domain::LayerType::Tile;
    }
}

namespace Pipeline {

    IoError SaveJson(const std::string& path, const Domain::TileMap& map) {
        try {
            json j;
            j["version"] = map.version;
            j["map"] = {
                {"name", map.name},
                {"width", map.width},
                {"height", map.height},
                {"tileSize", map.tileSize}
            };
            j["tileset"] = {
                {"image", map.tileset.imagePath},
                {"tileWidth", map.tileset.tileWidth},
                {"tileHeight", map.tileset.tileHeight},
                {"columns", map.tileset.columns},
                {"count", map.tileset.count}
            };

            j["layers"] = json::array();
            for (const auto& layer : map.layers) {
                json lj;
                lj["name"] = layer.name;
                lj["type"] = ToString(layer.type);
                lj["visible"] = layer.visible;
                lj["data"] = layer.data;
                j["layers"].push_back(lj);
            }

            std::ofstream ofs(path, std::ios::binary);
            if (!ofs) {
                return {false, "Failed to open for write: " + path};
            }
            ofs << j.dump(2);
            return {true, ""};
        } catch (const std::exception& e) {
            return {false, std::string("SaveJson exception: ") + e.what()};
        }
    }

    IoError LoadJson(const std::string& path, Domain::TileMap& out) {
        try {
            std::ifstream ifs(path, std::ios::binary);
            if (!ifs) return {false, "Failed to open for read: " + path};
            json j; ifs >> j;

            out.version = j.value("version", 1u);

            auto mj = j.at("map");
            out.name = mj.value("name", "stage_01");
            out.width = mj.value("width", 64u);
            out.height = mj.value("height", 36u);
            out.tileSize = mj.value("tileSize", 16u);

            auto tj = j.at("tileset");
            out.tileset.imagePath = tj.value("image", "");
            out.tileset.tileWidth = tj.value("tileWidth", 16u);
            out.tileset.tileHeight = tj.value("tileHeight", 16u);
            out.tileset.columns = tj.value("columns", 16u);
            out.tileset.count = tj.value("count", 256u);

            out.layers.clear();
            for (auto& lj : j.at("layers")) {
              Domain::Layer layer;
              layer.name = lj.value("name", "Layer");
              layer.type = ToLayerType(lj.value("type", "Tile"));
              layer.visible = lj.value("visible", true);
              layer.data = lj.value("data", std::vector<int32_t>{});
              out.layers.push_back(std::move(layer));
            }

            out.EnsureLayerDataSizes();
            return {true, ""};
        } catch (const std::exception& e) {
            return {false, std::string("LoadJson exception: ") + e.what()};
        }
    }

} // Pipeline