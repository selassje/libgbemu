find_package(Catch2 REQUIRED CONFIG)

add_executable(gb-core-tests)

target_sources(gb-core-tests PRIVATE gameboy_test.cpp)

target_link_libraries(gb-core-tests PRIVATE gbemu Catch2::Catch2WithMain)

setup_compiler_warnings(gb-core-tests)
setup_clang_tidy(gb-core-tests)
setup_msvc_analysis(gb-core-tests)

include(Catch)
catch_discover_tests(gb-core-tests)
