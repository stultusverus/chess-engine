cmake_minimum_required(VERSION 3.16)
project(chess-engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find nlohmann_json (header-only)
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(json)
endif()

# Engine library
add_library(engine STATIC
    src/engine/types.h
    src/engine/types.cpp
    src/engine/attacks.h
    src/engine/attacks.cpp
    src/engine/board.h
    src/engine/board.cpp
    src/engine/movegen.h
    src/engine/movegen.cpp
    src/engine/eval.h
    src/engine/eval.cpp
    src/engine/search.h
    src/engine/search.cpp
    src/engine/tt.h
    src/engine/tt.cpp
    src/engine/uci.h
    src/engine/uci.cpp
)

target_include_directories(engine PUBLIC src)
target_link_libraries(engine PUBLIC nlohmann_json::nlohmann_json)

# Stub main for compile-check
add_executable(chess-engine src/main.cpp)
target_link_libraries(chess-engine PRIVATE engine)

# Tests
enable_testing()

add_executable(test_board
    test/test_board.cpp
)

target_link_libraries(test_board PRIVATE engine)
add_test(NAME board COMMAND test_board)

add_executable(test_movegen
    test/test_movegen.cpp
)

target_link_libraries(test_movegen PRIVATE engine)
add_test(NAME movegen COMMAND test_movegen)
