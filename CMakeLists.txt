cmake_minimum_required(VERSION 3.14)
project(cartesian LANGUAGES CXX)

# --- Library target ---
# cartesian is a compiled library, not header-only: cartesian.cc carries the
# datum tables and the WGS84 / geodesy conversions, so it must be compiled and
# linked. The header alone (cartesian.h) only declares the Cartesian type and
# the self-contained CartesianError exception.
#
# There are NO third-party dependencies: the extraction severed both of the
# original Xapiand couplings (the located-exception THROW/CartesianError and
# strings::format), so this builds against the standard library alone. The
# header keeps its original Xapiand name so a consumer that already
# `#include "cartesian.h"` just needs this repo on its include path.
add_library(cartesian cartesian.cc)
target_include_directories(cartesian PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# C++20 for std::format (used in to_string() and the error messages), requested
# PUBLIC to stay uniform with the sibling libraries.
target_compile_features(cartesian PUBLIC cxx_std_20)

# Alias so consumers can link `cartesian::cartesian` whether they add this via
# add_subdirectory or FetchContent.
add_library(cartesian::cartesian ALIAS cartesian)

# Only build the test and demo when this repo is the top-level project, so
# consumers (e.g. via FetchContent / add_subdirectory) don't build them.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(cartesian_test test/test.cc)
  target_link_libraries(cartesian_test PRIVATE cartesian)
  add_test(NAME cartesian COMMAND cartesian_test)

  # A runnable tour of the library (not a test): build & run ./build/cartesian_demo
  add_executable(cartesian_demo examples/demo.cc)
  target_link_libraries(cartesian_demo PRIVATE cartesian)
endif()
