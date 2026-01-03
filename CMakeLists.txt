cmake_minimum_required(VERSION 3.21)
project(bulkchestopen VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE src)

if (DEFINED ENV{GEODE_SDK})
    add_subdirectory("$ENV{GEODE_SDK}" ${CMAKE_BINARY_DIR}/geode-sdk)
else()
    message(FATAL_ERROR "GEODE_SDK environment variable not set.")
endif()

setup_geode_mod(${PROJECT_NAME})

