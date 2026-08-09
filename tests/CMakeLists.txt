find_package(Catch2 REQUIRED CONFIG)

include(${CMAKE_SOURCE_DIR}/cmake/fetch_test_roms.cmake)

add_executable(gbemu-tests)

target_sources(
  gbemu-tests
  PRIVATE FILE_SET
          modules
          TYPE
          CXX_MODULES
          BASE_DIRS
          ${CMAKE_CURRENT_SOURCE_DIR}
          FILES
          ${CMAKE_CURRENT_SOURCE_DIR}/png.cppm)

if(MSVC)
  # See src/CMakeLists.txt's own copy of this workaround - VS IntelliSense's
  # design-time build reconstruction drops the /interface flag MSBuild's real
  # build infers from FILE_SET CXX_MODULES, misdiagnosing this file's `export`
  # as C3378.
  set_source_files_properties(png.cppm PROPERTIES COMPILE_OPTIONS /interface)
endif()

target_sources(gbemu-tests PRIVATE gameboy_test.cpp png.cpp)

target_link_libraries(gbemu-tests PRIVATE libgbemu Catch2::Catch2WithMain)

target_compile_definitions(
  gbemu-tests
  PRIVATE
    # DMG_ACID2_EXPECTED_DIR/CGB_ACID2_EXPECTED_DIR still point at a small
    # vendored dir under tests/ - reference.rgb is a raw dump of this project's
    # own frame buffer, not something upstream (or the fetched zip below)
    # provides. The ROMs themselves are unmodified upstream binaries, so those
    # come from the fetched zip like every other suite.
    DMG_ACID2_EXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/dmg-acid2"
    CGB_ACID2_EXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/cgb-acid2"
    MBC3_TESTER_EXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/mbc3-tester"
    # Fetched at configure time by cmake/fetch_test_roms.cmake - not vendored in
    # git, see that file for why.
    GB_TEST_ROMS_DIR="${TEST_ROMS_DIR}/blargg"
    DMG_ACID2_DIR="${TEST_ROMS_DIR}/dmg-acid2"
    CGB_ACID2_DIR="${TEST_ROMS_DIR}/cgb-acid2"
    AGE_TEST_ROMS_DIR="${TEST_ROMS_DIR}/age-test-roms"
    BULLY_DIR="${TEST_ROMS_DIR}/bully"
    CGB_ACID_HELL_DIR="${TEST_ROMS_DIR}/cgb-acid-hell"
    GAMBATTE_DIR="${TEST_ROMS_DIR}/gambatte"
    GBMICROTEST_DIR="${TEST_ROMS_DIR}/gbmicrotest"
    LITTLE_THINGS_GB_DIR="${TEST_ROMS_DIR}/little-things-gb"
    MBC3_TESTER_DIR="${TEST_ROMS_DIR}/mbc3-tester"
    MEALYBUG_TEAROOM_TESTS_DIR="${TEST_ROMS_DIR}/mealybug-tearoom-tests"
    MOONEYE_TEST_SUITE_DIR="${TEST_ROMS_DIR}/mooneye-test-suite"
    MOONEYE_TEST_SUITE_WILBERTPOL_DIR="${TEST_ROMS_DIR}/mooneye-test-suite-wilbertpol"
    RTC3TEST_DIR="${TEST_ROMS_DIR}/rtc3test"
    SAME_SUITE_DIR="${TEST_ROMS_DIR}/same-suite"
    SCRIBBLTESTS_DIR="${TEST_ROMS_DIR}/scribbltests"
    STRIKETHROUGH_DIR="${TEST_ROMS_DIR}/strikethrough"
    TURTLE_TESTS_DIR="${TEST_ROMS_DIR}/turtle-tests")

setup_compiler_warnings(gbemu-tests)
setup_clang_tidy(gbemu-tests)
setup_msvc_analysis(gbemu-tests)
setup_coverage_report_flags(gbemu-tests)
setup_sanitizers(gbemu-tests)
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
