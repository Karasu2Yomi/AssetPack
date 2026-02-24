#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <filesystem>

namespace UI {

    // tileset を SDL_Texture としてキャッシュする（SDL_Renderer用）
    struct TilesetTexture {
        SDL_Texture* tex = nullptr;
        int w = 0;
        int h = 0;
        std::string loadedAbsPath;

        void Reset();
    };

    // 相対パス（Project基準）/ 絶対パスの両方を受けて、Textureをキャッシュし返す
    // 成功: true（out.tex != nullptr）
    // 失敗: false（outErr に理由）
    bool GetOrLoadTilesetTexture(
        SDL_Renderer* renderer,
        const std::filesystem::path& projectRootAbs,   // 例：exeDir/Project
        const std::string& tilesetPathRelOrAbs,        // 例："tilesets/basic.png"
        TilesetTexture& cache,
        std::string* outErr = nullptr);

    // 終了時に呼ぶ（リーク回避）
    void ReleaseTilesetTexture(TilesetTexture& cache);

} // namespace UI