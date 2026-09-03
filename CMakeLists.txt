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

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------
# Testing
# -----------------------------
include(CTest)

# -----------------------------
# third_party
# -----------------------------
add_subdirectory(third_party)

set(ASSETPACK_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/assetpack_core/CsvCodec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/assetpack_core/PuzzleProjectStore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/assetpack_core/PuzzleResolver.cpp
)

add_library(assetpack_core STATIC
    ${ASSETPACK_CORE_SOURCES}
)

target_include_directories(assetpack_core
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if (BUILD_TESTING AND ASSETPACK_BUILD_TESTS)
    add_executable(assetpack_editor_tests
        tests/PuzzleEditorTests.cpp
        tests/UiInteractionTests.cpp
        src/app/EditorApp.cpp
        src/app/Logger.cpp
        src/platform/WinFileDialog.cpp
        src/ui/TilesetTextureCache.cpp
        src/ui/UiProject.cpp
        src/ui/UiViewport.cpp
        src/ui/UiInspector.cpp
    )
    target_include_directories(assetpack_editor_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(assetpack_editor_tests PRIVATE
        assetpack_core
        doctest::doctest
        imgui_lib
        SDL3_image::SDL3_image
    )
    if (WIN32)
        target_link_libraries(assetpack_editor_tests PRIVATE ole32 uuid)
    endif()
    add_test(NAME assetpack_editor_tests COMMAND assetpack_editor_tests)
endif()

set(ASSETPACK_APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/EditorApp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/WinFileDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/util/PathUtil.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/TilesetTextureCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/UiRoot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/UiProject.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/UiViewport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/UiInspector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/UiBottomTabs.cpp
)

add_executable(${PROJECT_NAME}
    ${ASSETPACK_APP_SOURCES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "AssetPack"
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
    assetpack_core
)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ole32 uuid)
endif()

if (MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE -mconsole)
endif()

if (WIN32 AND MSVC AND ASSETPACK_COPY_RUNTIME_DLLS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND_EXPAND_LISTS
    )
endif()
