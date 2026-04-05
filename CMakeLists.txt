cmake_minimum_required(VERSION 3.20)
project(hft-backtest-engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(hft-backtest-engine
    src/main.cpp
    src/csv_data_loader.cpp
)

target_include_directories(hft-backtest-engine PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_compile_options(hft-backtest-engine PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow -Wnull-dereference
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(hft-backtest-engine PRIVATE
        -O0 -g3 -fsanitize=address,undefined -DDEBUG
    )
    target_link_options(hft-backtest-engine PRIVATE
        -fsanitize=address,undefined
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(hft-backtest-engine PRIVATE -O3 -march=native)
    target_compile_definitions(hft-backtest-engine PRIVATE NDEBUG)

    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported)
    if(ipo_supported)
        set_property(TARGET hft-backtest-engine PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

# add_subdirectory(tests)