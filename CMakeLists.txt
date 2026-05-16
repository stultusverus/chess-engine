cmake_minimum_required(VERSION 3.16)
project(chess-engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED CHESS_ENGINE_VERSION)
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --always
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE CHESS_ENGINE_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()
endif()

if(NOT CHESS_ENGINE_VERSION)
    set(CHESS_ENGINE_VERSION "unknown")
endif()

message(STATUS "Chess engine version: ${CHESS_ENGINE_VERSION}")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/engine/version.h
    @ONLY
)

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
    src/engine/see.h
    src/engine/see.cpp
    src/engine/search.h
    src/engine/search.cpp
    src/engine/tt.h
    src/engine/tt.cpp
    src/engine/book.h
    src/engine/book.cpp
    src/engine/uci.h
    src/engine/uci.cpp
    src/engine/tune.h
    src/engine/tune.cpp
)

target_include_directories(engine PUBLIC src)
target_include_directories(engine PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/generated)

# UCI engine executable
add_executable(chess-engine src/main.cpp)
target_link_libraries(chess-engine PRIVATE engine)

add_executable(bench_engine tools/bench.cpp)
target_link_libraries(bench_engine PRIVATE engine)

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

add_executable(test_see test/test_see.cpp)
target_link_libraries(test_see PRIVATE engine)
add_test(NAME see COMMAND test_see)

add_executable(test_book test/test_book.cpp)
target_link_libraries(test_book PRIVATE engine)
add_test(NAME book COMMAND test_book)

add_executable(test_uci test/test_uci.cpp)
add_dependencies(test_uci chess-engine)
add_test(NAME uci COMMAND test_uci)

add_executable(test_tune test/test_tune.cpp)
target_link_libraries(test_tune PRIVATE engine)
add_test(NAME tune COMMAND test_tune)

# EPD tactical fixture smoke test
add_test(NAME epd_fixture COMMAND bench_engine ${CMAKE_SOURCE_DIR}/test/fixtures/tactical.epd)
# Eval trace smoke test
add_test(NAME eval_trace COMMAND bench_engine trace rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 --json)

# Tune-eval fixture smoke test
add_test(NAME tune_fixture COMMAND bench_engine tune-eval ${CMAKE_SOURCE_DIR}/test/fixtures/tune-small.txt --json)

# Bench signature verification (skipped by default; set VERIFY_BENCH_SIGNATURE=1 to enable)
add_test(NAME bench_signature COMMAND ${CMAKE_SOURCE_DIR}/scripts/verify-bench-signature.sh)
set_property(TEST bench_signature PROPERTY ENVIRONMENT "BENCH_BIN=$<TARGET_FILE:bench_engine>")
