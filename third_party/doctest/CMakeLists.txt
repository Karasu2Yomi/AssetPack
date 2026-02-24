include(FetchContent)

# 依存物の置き場所（build/_deps 配下などに揃う）
set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/../third_party/doctest" CACHE PATH "" FORCE)

# 再現性重視：勝手に更新しない（初回DLは必要）
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)


# ----------------------------
# doctest
# ----------------------------
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.12
)

# doctest 本体のサンプル/テスト等をビルドしない（オプション名は版で差があるので必要時だけ）
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(DOCTEST_WITH_MAIN OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(doctest)