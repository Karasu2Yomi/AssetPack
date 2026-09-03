#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>

namespace UI {

struct TextureData {
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
    std::string path;

    void Reset();
};

struct TextureCache {
    std::unordered_map<std::string, TextureData> cache;

    bool GetOrLoad(SDL_Renderer* renderer, const std::string& absoluteOrRelative,
                  const std::string& resourceRoot, TextureData*& out,
                  std::string* outErr = nullptr);

    void Release(const std::string& key);
    void ReleaseAll();
};

} // namespace UI
