cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0097 NEW)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(gmdsync VERSION 1.0.0)

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN)
endif()

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})

if (WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32)
endif()
