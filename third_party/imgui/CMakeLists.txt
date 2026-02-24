include(FetchContent)

# 依存物の置き場所（build/_deps 配下などに揃う）
set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/../third_party/imgui" CACHE PATH "" FORCE)

# 再現性重視：勝手に更新しない（初回DLは必要）
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

# ----------------------------
# Dear ImGui
# ----------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.5-docking
)

FetchContent_MakeAvailable(imgui)

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp

    # SDL3 platform backend
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    # SDL_Renderer (SDL3) renderer backend
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp

    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)

target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_SOURCE_DIR}/misc/cpp
)

target_link_libraries(imgui_lib PUBLIC
    SDL3::SDL3
)

add_library(imgui::imgui ALIAS imgui_lib)

#target_compile_definitions(imgui_lib PUBLIC IMGUI_ENABLE_DOCKING)