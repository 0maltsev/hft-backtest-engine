cmake_minimum_required(VERSION 3.20)
project(hft-backtest-engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(COMMON_WARNINGS -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow -Wnull-dereference)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG -flto")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -fsanitize=address,undefined -DDEBUG")
endif()

add_executable(hft-backtest-engine src/main.cpp)
target_include_directories(hft-backtest-engine PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(hft-backtest-engine PRIVATE ${COMMON_WARNINGS})

# add_subdirectory(tests)