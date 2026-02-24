include(FetchContent)

# 依存物の置き場所（build/_deps 配下などに揃う）
set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/../third_party/nlohmann_json" CACHE PATH "" FORCE)

# 再現性重視：勝手に更新しない（初回DLは必要）
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)


# ----------------------------
# nlohmann/json
# ----------------------------

# テスト不要なら（バージョンによりオプション名が変わる可能性あり）
# set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)
