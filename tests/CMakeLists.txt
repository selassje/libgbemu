# This directory is add_subdirectory()'d (see the top-level CMakeLists.txt)
# whenever ENABLE_TESTS or ENABLE_FUZZING is on, since fuzzing.cpp/gbemu-fuzz
# below live alongside the Catch2 suite rather than under a separate top-level
# dir - but the two are independently gated: fuzzing doesn't need Catch2 or the
# fetched test-ROM corpus, and vice versa.
if(ENABLE_TESTS)
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
            ${CMAKE_CURRENT_SOURCE_DIR}/png.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/test_helpers.cppm)

  if(MSVC)
    # See src/CMakeLists.txt's own copy of this workaround - VS IntelliSense's
    # design-time build reconstruction drops the /interface flag MSBuild's real
    # build infers from FILE_SET CXX_MODULES, misdiagnosing this file's `export`
    # as C3378.
    set_source_files_properties(png.cppm test_helpers.cppm
                                PROPERTIES COMPILE_OPTIONS /interface)
  endif()

  # One file per component (mirroring the :cpu/:apu/:ppu/:mmu module partition
  # split libgbemu itself already uses), plus gameboy_test.cpp for the top-level
  # GameBoy API surface that doesn't belong to any one component -
  # gameboy_test.cpp held every TEST_CASE here until it passed ~980 lines.
  # test_helpers.cpp/.cppm holds the readFile()/stabilizeAndGetFrame()/etc.
  # helpers every one of these files imports instead of each keeping its own
  # copy.
  target_sources(
    gbemu-tests
    PRIVATE gameboy_test.cpp
            cpu_test.cpp
            apu_test.cpp
            ppu_test.cpp
            mapper_test.cpp
            mmu_test.cpp
            serialization_test.cpp
            test_helpers.cpp
            png.cpp)

  target_link_libraries(gbemu-tests PRIVATE libgbemu Catch2::Catch2WithMain)

  target_compile_definitions(
    gbemu-tests
    PRIVATE
      # DMG_ACID2_EXPECTED_DIR/CGB_ACID2_EXPECTED_DIR still point at a small
      # vendored dir under tests/ - reference.rgb is a raw dump of this
      # project's own frame buffer, not something upstream (or the fetched zip
      # below) provides. The ROMs themselves are unmodified upstream binaries,
      # so those come from the fetched zip like every other suite.
      DMG_ACID2_EXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/dmg-acid2"
      CGB_ACID2_EXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/cgb-acid2"
      MBC3_TESTER_EXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/mbc3-tester"
      # mooneye-test-suite doesn't ship its own reference screenshots the way
      # game-boy-test-roms does for rtc3test/mbc3-tester - self-captured PNGs
      # (via png.cpp's writePixelsAsPng(), same as dmg-acid2/cgb-acid2's own
      # reference.rgb) instead.
      MBC1_TEST_EXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/mbc1"
      MBC2_TEST_EXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/mbc2"
      MBC5_TEST_EXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/mbc5"
      # Fetched at configure time by cmake/fetch_test_roms.cmake - not vendored
      # in git, see that file for why.
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
  setup_lto(gbemu-tests)

  if(ENABLE_MSVC_ANALYSIS)
    # Test cases construct gbemu::GameBoy (which owns the ~58KB Mmu memory map)
    # on the stack, tripping C6262. That's fine for test binaries - applies to
    # every split file that does this except test_helpers.cpp (takes GameBoy& by
    # reference, never constructs one) and png.cpp (doesn't touch GameBoy at
    # all).
    set_source_files_properties(
      gameboy_test.cpp
      cpu_test.cpp
      apu_test.cpp
      ppu_test.cpp
      mapper_test.cpp
      mmu_test.cpp
      serialization_test.cpp
      PROPERTIES COMPILE_OPTIONS "/wd6262")
  endif()

  include(Catch)
  if(ENABLE_COVERAGE_REPORT)
    # Catch2's test discovery runs gbemu-tests once per TEST_CASE, all from the
    # same binary; %p (process id) keeps each run's profile data separate so
    # they can all be merged afterwards by generate-coverage-report.
    catch_discover_tests(
      gbemu-tests PROPERTIES ENVIRONMENT
      "LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/coverage/%p.profraw")
  else()
    catch_discover_tests(gbemu-tests)
  endif()
endif()

if(ENABLE_FUZZING)
  # -fsanitize=fuzzer (what setup_fuzzer() adds) is a Clang-only flag in this
  # project's toolchains - GCC's own libFuzzer support isn't wired up here, so
  # fail early with a clear message rather than a confusing flag-not-recognized
  # error partway through configuring this target.
  if(NOT ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    message(
      FATAL_ERROR
        "ENABLE_FUZZING requires Clang (libFuzzer, via -fsanitize=fuzzer, isn't available from ${CMAKE_CXX_COMPILER_ID} in this project's toolchains) - use a Clang preset instead."
    )
  endif()

  # Confirmed directly: LLVM's prebuilt clang_rt.fuzzer-x86_64.lib on Windows is
  # only built against the static release CRT (/MT), while this project's Conan
  # toolchain forces the dynamic CRT (/MD) everywhere (derived from the Conan
  # profile's own compiler.runtime setting, not overridable via a plain CMake
  # cache variable from a preset) - linking fails with an lld-link
  # /failifmismatch RuntimeLibrary error. Not worth chasing a Conan-profile fix
  # for right now (see release_ninja_clang_fuzzing_linux, which works cleanly -
  # Linux has no such CRT-variant concept at all), so fail clearly here instead
  # of leaving a Windows+Clang user to hit that confusing linker error partway
  # through a build.
  if(WIN32)
    message(
      FATAL_ERROR
        "ENABLE_FUZZING isn't supported on Windows yet (LLVM's prebuilt Windows libFuzzer runtime requires a static-CRT build this project's Conan toolchain doesn't produce) - use release_ninja_clang_fuzzing_linux instead."
    )
  endif()

  # Deliberately not part of the gbemu-tests binary/ctest suite above (an
  # entirely separate executable, only even built when ENABLE_FUZZING is on) -
  # libFuzzer provides its own main() via -fsanitize=fuzzer, which would
  # conflict with Catch2WithMain's.
  add_executable(gbemu-fuzz fuzzing.cpp)

  target_link_libraries(gbemu-fuzz PRIVATE libgbemu)

  setup_compiler_warnings(gbemu-fuzz)
  setup_clang_tidy(gbemu-fuzz)
  setup_std_lib(gbemu-fuzz)
  setup_fuzzer(gbemu-fuzz)
  # Not gated behind its own ENABLE_SANITIZERS check here - already gated inside
  # setup_sanitizers() itself, same as every other target that calls it.
  # Combining ASan/UBSan with libFuzzer (rather than -fsanitize=fuzzer alone) is
  # what actually makes fuzzing effective at finding memory-safety/ UB bugs, not
  # just crashes - see setup_fuzzer()'s own comment on why that combination
  # lives here instead of being hardcoded into setup_fuzzer() itself.
  setup_sanitizers(gbemu-fuzz)
endif()
