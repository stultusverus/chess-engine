cmake_minimum_required(VERSION 3.16)
project(chess-engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find dependencies
find_package(CURL REQUIRED)

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
    src/engine/book.h
    src/engine/book.cpp
    src/engine/uci.h
    src/engine/uci.cpp
)

target_include_directories(engine PUBLIC src)
target_link_libraries(engine PUBLIC nlohmann_json::nlohmann_json)

# Bot library
add_library(bot STATIC
    src/bot/client.h
    src/bot/client.cpp
    src/bot/manager.h
    src/bot/manager.cpp
)

target_include_directories(bot PUBLIC src)
target_link_libraries(bot PUBLIC CURL::libcurl nlohmann_json::nlohmann_json engine)

# UCI engine executable
add_executable(chess-engine src/main.cpp)
target_link_libraries(chess-engine PRIVATE engine)

# Bot executable
add_executable(chess-bot src/bot_main.cpp)
target_link_libraries(chess-bot PRIVATE bot engine)

# Tests
enable_testing()

add_executable(test_board test/test_board.cpp)
target_link_libraries(test_board PRIVATE engine)
add_test(NAME board COMMAND test_board)

add_executable(test_movegen test/test_movegen.cpp)
target_link_libraries(test_movegen PRIVATE engine)
add_test(NAME movegen COMMAND test_movegen)

add_executable(test_eval test/test_eval.cpp)
target_link_libraries(test_eval PRIVATE engine)
add_test(NAME eval COMMAND test_eval)

add_executable(test_search test/test_search.cpp)
target_link_libraries(test_search PRIVATE engine)
add_test(NAME search COMMAND test_search)
