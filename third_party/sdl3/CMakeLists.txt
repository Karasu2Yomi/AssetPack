include(FetchContent)

# 依存物の置き場所（build/_deps 配下などに揃う）
set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/../third_party/sdl3" CACHE PATH "" FORCE)

# 再現性重視：勝手に更新しない（初回DLは必要）
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)


# ----------------------------
# SDL3
# ----------------------------
# SDLのCMakeは SDL3::SDL3 ターゲットが保証されています
# ※ SDL3::SDL3-shared / SDL3::SDL3-static は保証されない :contentReference[oaicite:1]{index=1}
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
# SDL3_image が要求しがちな SDL3_test を確実に作る
set(SDL_TEST_LIBRARY ON CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.0
)
FetchContent_MakeAvailable(SDL3)