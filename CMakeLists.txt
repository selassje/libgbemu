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

include(cmake/compiler_and_linker_flags.cmake)
include(cmake/clang_format.cmake)
include(cmake/cmake_format.cmake)
include(cmake/setup_static_analysis.cmake)
include(cmake/coverage_report.cmake)

add_subdirectory(src)

if(ENABLE_TESTS)
  set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
  enable_testing()
  add_subdirectory(tests)
endif()

setup_coverage_report_target()
