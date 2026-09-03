#include "TilesetTextureCache.hpp"

#include <SDL3_image/SDL_image.h>
#include <filesystem>

namespace {
std::string ToAbsolute(std::string_view resourceRoot,
                       std::string_view pathLike) {
    const std::filesystem::path p{pathLike};
    if (p.is_absolute()) {
        return p.generic_string();
    }
    return (std::filesystem::path{resourceRoot} / p).generic_string();
}
} // namespace

namespace UI {

void TextureData::Reset() {
    if (texture) {
        SDL_DestroyTexture(texture);
    }
    texture = nullptr;
    width = 0;
    height = 0;
    path.clear();
}

bool TextureCache::GetOrLoad(SDL_Renderer* renderer,
                            const std::string& absoluteOrRelative,
                            const std::string& resourceRoot,
                            TextureData*& out,
                            std::string* outErr) {
    const auto abs = ToAbsolute(resourceRoot, absoluteOrRelative);
    const auto it = cache.find(abs);
    if (it != cache.end()) {
        out = &it->second;
        return true;
    }

    if (!renderer) {
        if (outErr) *outErr = "renderer is null";
        return false;
    }

    SDL_Surface* surface = IMG_Load(abs.c_str());
    if (!surface) {
        if (outErr) {
            *outErr = "SDL_image load failed: " + std::string(SDL_GetError());
        }
        return false;
    }

    TextureData data;
    data.width = surface->w;
    data.height = surface->h;
    data.texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (!data.texture) {
        if (outErr) {
            *outErr = "create texture failed: " + std::string(SDL_GetError());
        }
        return false;
    }
    data.path = abs;
    SDL_SetTextureBlendMode(data.texture, SDL_BLENDMODE_BLEND);

    auto inserted = cache.emplace(abs, data);
    out = &inserted.first->second;
    return true;
}

void TextureCache::Release(const std::string& key) {
    const auto it = cache.find(key);
    if (it == cache.end()) return;
    it->second.Reset();
    cache.erase(it);
}

void TextureCache::ReleaseAll() {
    for (auto& pair : cache) {
        pair.second.Reset();
    }
    cache.clear();
}

} // namespace UI
