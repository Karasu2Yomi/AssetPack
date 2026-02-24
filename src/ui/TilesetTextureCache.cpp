#include "TilesetTextureCache.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include "util/PathUtil.hpp"

namespace UI {


    void TilesetTexture::Reset() {
        if (tex) SDL_DestroyTexture(tex);
        tex = nullptr;
        w = h = 0;
        loadedAbsPath.clear();
    }

    void ReleaseTilesetTexture(TilesetTexture& cache) {
        cache.Reset();
    }

    static std::filesystem::path ResolveAbs(const std::filesystem::path& projectRootAbs,
                                           const std::string& relOrAbs) {
        std::filesystem::path p(relOrAbs);
        if (p.is_absolute()) return p;
        return projectRootAbs / p; // Project基準
    }

    bool GetOrLoadTilesetTexture(SDL_Renderer* renderer,
                                 const std::filesystem::path& projectRootAbs,
                                 const std::string& tilesetPathRelOrAbs,
                                 TilesetTexture& cache,
                                 std::string* outErr){

        if (!renderer) {
            if (outErr) *outErr = "renderer is null";
            return false;
        }
        if (tilesetPathRelOrAbs.empty()) {
            if (outErr) *outErr = "tileset path is empty";
            return false;
        }

        const std::filesystem::path absPath = ResolveAbs(projectRootAbs, tilesetPathRelOrAbs);
        const std::string absUtf8 = Util::ToUtf8(absPath);

        // 同じパスなら再ロードしない
        if (cache.tex && cache.loadedAbsPath == absUtf8) {
            return true;
        }

        cache.Reset();

        SDL_Surface* surf = IMG_Load(absUtf8.c_str());
        if (!surf) {
            if (outErr) *outErr = std::string("IMG_Load failed: ") +
                (SDL_GetError() ? SDL_GetError() : "") + " path=" + absUtf8;
            return false;
        }

        cache.w = surf->w;
        cache.h = surf->h;

        cache.tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_DestroySurface(surf);

        if (!cache.tex) {
            if (outErr) *outErr = std::string("SDL_CreateTextureFromSurface failed: ") +
                                  (SDL_GetError() ? SDL_GetError() : "") + " path=" + absUtf8;
            cache.Reset();
            return false;
        }

        SDL_SetTextureBlendMode(cache.tex, SDL_BLENDMODE_BLEND);
        cache.loadedAbsPath = absUtf8;
        return true;
    }

} // namespace UI