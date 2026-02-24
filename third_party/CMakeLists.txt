# =========================
# [third_party 導入位置] ここで各 3rd party を add_subdirectory
# =========================
# SDL3（submodule等で third_party/SDL3 に置いた）
# add_subdirectory(sdl3)

# add_subdirectory(sdl3_image)

# =========================
# SDL3 (vendored)
# =========================
add_subdirectory(sdl3)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/sdl3
    ${CMAKE_CURRENT_BINARY_DIR}/sdl3-build
    EXCLUDE_FROM_ALL)


# =========================
# SDL3_image (vendored)
# =========================
add_subdirectory(sdl3_image)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/sdl3_image/sdl3_image-src
    ${CMAKE_CURRENT_BINARY_DIR}/sdl3_image-build
    EXCLUDE_FROM_ALL)


# ターゲット名の揺れ吸収（環境によって -static/-shared が出ることがある）
if (NOT TARGET SDL3_image::SDL3_image)
    if (TARGET SDL3_image-static)
        add_library(SDL3_image::SDL3_image ALIAS SDL3_image-static)
    elseif (TARGET SDL3_image-shared)
        add_library(SDL3_image::SDL3_image ALIAS SDL3_image-shared)
    endif()
endif()


# nlohmann/json（header-onlyだが CMake target を出してくれる）
add_subdirectory(nlohmann_json)

# ImGui（third_party/imgui/CMakeLists.txt を使う）
add_subdirectory(imgui)

# doctest（third_party/doctest/CMakeLists.txt を使う）
# add_subdirectory(doctest)

