find_package(Catch2 REQUIRED CONFIG)

add_executable(gbemu-tests)

target_sources(gbemu-tests PRIVATE gameboy_test.cpp)

target_link_libraries(gbemu-tests PRIVATE gbemu Catch2::Catch2WithMain)

target_compile_definitions(
  gbemu-tests
  PRIVATE GB_TEST_ROMS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/gb-test-roms")

setup_compiler_warnings(gbemu-tests)
setup_clang_tidy(gbemu-tests)
setup_msvc_analysis(gbemu-tests)
setup_coverage_report_flags(gbemu-tests)
setup_std_lib(gbemu-tests)

if(ENABLE_MSVC_ANALYSIS)
  # Test cases construct gbemu::GameBoy (which owns the ~58KB Mmu memory map) on
  # the stack, tripping C6262. That's fine for test binaries.
  set_source_files_properties(gameboy_test.cpp PROPERTIES COMPILE_OPTIONS
                                                          "/wd6262")
endif()

include(Catch)
if(ENABLE_COVERAGE_REPORT)
  # Catch2's test discovery runs gbemu-tests once per TEST_CASE, all from the
  # same binary; %p (process id) keeps each run's profile data separate so they
  # can all be merged afterwards by generate-coverage-report.
  catch_discover_tests(
    gbemu-tests PROPERTIES ENVIRONMENT
    "LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/coverage/%p.profraw")
else()
  catch_discover_tests(gbemu-tests)
endif()
