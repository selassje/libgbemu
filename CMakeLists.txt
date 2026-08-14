cmake_minimum_required(VERSION 3.30)

include(cmake/bootstrap_conan.cmake)

project(libgbemu LANGUAGES CXX)

# The Conan toolchain (included by project()) forces CMAKE_CXX_STANDARD to 14
# for Catch2's own build requirements; set ours afterwards so it wins. NOTE:
# C++26 is the target per project decisions, but CMake 4.2 does not yet map
# dialect 26 to an MSVC flag, so this is pinned to 23 until that lands.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

option(ENABLE_TESTS "Enable Tests" OFF)
option(ENABLE_CLANG_TIDY "Enable Clang Tidy" OFF)
option(ENABLE_MSVC_ANALYSIS "Enable MSVC Analysis" OFF)
option(ENABLE_COVERAGE_REPORT "Enable Test Coverage Report" OFF)
option(ENABLE_SANITIZERS "Enable Sanitizers" OFF)
option(ENABLE_FUZZING "Enable LLVM libFuzzer fuzz targets (Clang only)" OFF)

include(cmake/compiler_and_linker_flags.cmake)
include(cmake/setup_static_analysis.cmake)

# clang-format/cmake-format define fixed-name global targets, and
# coverage_report.cmake's target walks tests/ via CMAKE_SOURCE_DIR - both only
# make sense (and only resolve paths correctly) when libgbemu is the outermost
# project being configured, not when it's add_subdirectory()'d into something
# else's build (e.g. the gbemu frontend), which would otherwise collide with
# that project's own same-named targets.
if(PROJECT_IS_TOP_LEVEL)
  include(cmake/clang_format.cmake)
  include(cmake/cmake_format.cmake)
  include(cmake/coverage_report.cmake)
endif()

add_subdirectory(src)

if(ENABLE_TESTS)
  set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
  enable_testing()
endif()

# tests/ also holds fuzzing.cpp/the gbemu-fuzz target (see
# tests/CMakeLists.txt), gated on ENABLE_FUZZING independently of ENABLE_TESTS
# in there - so this needs to add_subdirectory() it whenever either is on, not
# just ENABLE_TESTS.
if(ENABLE_TESTS OR ENABLE_FUZZING)
  add_subdirectory(tests)
endif()

if(PROJECT_IS_TOP_LEVEL)
  setup_coverage_report_target()
endif()
