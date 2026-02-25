cmake_minimum_required(VERSION 3.30)
project(AssetPack LANGUAGES C CXX)

# -----------------------------
# Options
# -----------------------------
option(ASSETPACK_COPY_RUNTIME_DLLS "Copy runtime DLLs next to exe" OFF)
option(ASSETPACK_BUILD_TESTS "Build tests" ON)

# -----------------------------
# C++ Standard
# -----------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# (Optional) CLion向け
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------
# Testing
# -----------------------------
include(CTest) # BUILD_TESTING を定義して enable_testing() も行う
# if (ASSETPACK_BUILD_TESTS)
#   add_subdirectory(tests)
# endif()

# -----------------------------
# third_party
# -----------------------------
add_subdirectory(third_party)

# -----------------------------
# App executable
# -----------------------------
file(GLOB_RECURSE ASSETPACK_SRC CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

add_executable(${PROJECT_NAME}
    ${ASSETPACK_SRC}
)

# CI/YAML と合わせるため exe 名を固定
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "AssetPack-LUO_JIEWEN"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    imgui_lib
    nlohmann_json::nlohmann_json
)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ole32 uuid)
endif()

# MinGWで WinMain を要求しないように（保険）
if (MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE -mconsole)
endif()

# Windows: 実行時DLLコピー（ON のとき）
# TARGET_RUNTIME_DLLS は MinGW だと空になって POST_BUILD が壊れやすいので、
# ここでは MSVC 限定にしておく（MinGW の DLL 収集は CI の ldd で行う）
if (WIN32 AND MSVC AND ASSETPACK_COPY_RUNTIME_DLLS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND_EXPAND_LISTS
    )
endif()