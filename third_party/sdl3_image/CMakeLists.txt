include(FetchContent)

# 依存物の置き場所（build/_deps 配下などに揃う）
set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/../third_party/sdl3_image" CACHE PATH "" FORCE)

# 再現性重視：勝手に更新しない（初回DLは必要）
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

# SDL3_image が root project でない場合は通常 samples/tests はデフォルトOFF寄りだが、明示しておく
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TESTS   OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_INSTALL OFF CACHE BOOL "" FORCE)

# staticで揃える（SDL3もstaticにしているため）
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# ----------------------------
# SDL3_image
# ----------------------------
FetchContent_Declare(
    sdl3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-3.2.0
)
# FetchContent_MakeAvailable(sdl3_image)