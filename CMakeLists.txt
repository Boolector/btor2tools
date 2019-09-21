cmake_minimum_required(VERSION 3.3)

project(btor2tools)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(BUILD_BTOR2AIGER "Build btor2aiger binary" OFF)

if(BUILD_BTOR2AIGER)
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/deps/aiger/aiger.c")
    message(FATAL_ERROR
            "Could not find AIGER sources. Execute script setup-aiger.sh.")
  endif()
  list(APPEND CMAKE_PREFIX_PATH "${PROJECT_SOURCE_DIR}/deps/install")
  find_package(Boolector)
endif()

add_subdirectory(src)
