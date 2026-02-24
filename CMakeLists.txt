cmake_minimum_required(VERSION 3.30)
project(AssetPack LANGUAGES C CXX)

option(ASSETPACK_COPY_RUNTIME_DLLS "Copy runtime DLLs next to exe" OFF)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# (Optional) CLion向け: compile_commands.json を出す
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- third_party ----
add_subdirectory(third_party)

# ---- App executable ----
file(GLOB_RECURSE ASSETPACK_SRC CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

#add_executable(${PROJECT_NAME}
#    WIN32
#    src/main.cpp
#)
add_executable(${PROJECT_NAME}
    ${ASSETPACK_SRC}
)

target_include_directories(${PROJECT_NAME}  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)


#target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    imgui_lib
    nlohmann_json::nlohmann_json
)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ole32 uuid)
endif()

# MinGWで WinMain を要求しないように強制（保険）
if (MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE -mconsole)
endif()

# Windows: 実行時にSDL3.dllが必要な場合があるので、exeの隣にコピー
if (WIN32 AND ASSETPACK_COPY_RUNTIME_DLLS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND_EXPAND_LISTS
    )
endif()