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
      # Not vendored/tracked - pixelsMatchPng() (png.cpp) writes into this on
      # every mismatch, so a failing PNG-comparison test's actual frame can be
      # opened directly instead of only getting a pass/fail bool.
      TEST_OUTPUTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/outputs"
      MBC3_TESTER_EXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/mbc3-tester"
      # mooneye-test-suite doesn't ship its own reference screenshots the way
      # game-boy-test-roms does for rtc3test/mbc3-tester - self-captured PNGs
      # (via png.cpp's writePixelsAsPng(), same as dmg-acid2/cgb-acid2's own
      # reference.rgb) instead.
      MBC1_TEST_EXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/mbc1"
      MBC2_TEST_EXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/mbc2"
      MBC5_TEST_EXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/mbc5"
      MOONEYE_ACCEPTANCE_EXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/mooneye-acceptance"
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
  #
  # Two independent binaries, not one LLVMFuzzerTestOneInput that chains both
  # calls the way this used to work (fuzzing.cpp, now split into
  # fuzzing_load_rom.cpp/fuzzing_load_state.cpp) - loadRom() (a ROM image) and
  # loadState() (a serialized GameBoy save state) parse completely different
  # byte formats, so mutating one shared input against both wastes libFuzzer's
  # coverage-guided mutation on whichever input shape it isn't currently
  # exercising, and the two need entirely separate corpora (a ROM-shaped input
  # is never also going to look like a valid save state, and vice versa).
  add_executable(gbemu-fuzz-load-rom fuzzing_load_rom.cpp)
  add_executable(gbemu-fuzz-load-state fuzzing_load_state.cpp)

  foreach(fuzz_target gbemu-fuzz-load-rom gbemu-fuzz-load-state)
    target_link_libraries(${fuzz_target} PRIVATE libgbemu)

    setup_compiler_warnings(${fuzz_target})
    setup_clang_tidy(${fuzz_target})
    setup_std_lib(${fuzz_target})
    # libgbemu itself gets setup_lto()'d (see src/CMakeLists.txt), which under
    # Clang means its static library's .o members are LLVM bitcode, not plain
    # object code - linking against it from a target that *isn't* itself
    # LTO-enabled skips clang's LTO-aware link path entirely, and the plain
    # system linker can't read bitcode ("file format not recognized", hit
    # directly before adding this). Matching gbemu-tests' own
    # setup_lto(gbemu-tests) call above fixes it.
    setup_lto(${fuzz_target})
    setup_fuzzer(${fuzz_target})
    # Not gated behind its own ENABLE_SANITIZERS check here - already gated
    # inside setup_sanitizers() itself, same as every other target that calls
    # it. Combining ASan/UBSan with libFuzzer (rather than -fsanitize=fuzzer
    # alone) is what actually makes fuzzing effective at finding
    # memory-safety/UB bugs, not just crashes - see setup_fuzzer()'s own comment
    # on why that combination lives here instead of being hardcoded into
    # setup_fuzzer() itself.
    setup_sanitizers(${fuzz_target})
  endforeach()

  # Registered as real ctest tests (only when ENABLE_TESTS is also on -
  # add_test() is a no-op otherwise, since enable_testing() itself is only
  # called in that case, see the top-level CMakeLists.txt) rather than a
  # separate CI-only shell step, so `ctest --preset
  # release_ninja_clang_fuzzing_linux` picks fuzzing up the same way it already
  # does gbemu-tests.
  if(ENABLE_TESTS)
    include(ProcessorCount)
    ProcessorCount(NPROC)
    if(NPROC EQUAL 0)
      set(NPROC 1)
    endif()

    # tests/fuzz-corpus/ (writable) and tests/fuzz-artifacts/ (crash
    # reproducers) live under tests/, not the build dir - matching where
    # .github/workflows/ci.yml's own actions/cache and upload-artifact steps
    # already expect them, so this doesn't need a matching change there. Each
    # fuzz target gets its own subdirectory of both (load-rom/, load-state/) - a
    # ROM-shaped corpus entry and a save-state-shaped one share nothing, so
    # mixing them in one directory would just make each binary spend time
    # reading and re-minimizing inputs it can never actually use.
    # tests/fuzz-seeds/<target>/ is the matching 2nd, read-only corpus dir per
    # target (libFuzzer: "if more than one corpus dir is provided, the 1st is
    # used for output, the rest are read-only") - committed to git, so it's
    # always present even on a cold cache, unlike tests/fuzz-corpus/ itself.
    # Without at least one valid-shaped seed, a random buffer's length is
    # astronomically unlikely to ever land on an exact multiple of ROM_BANK_SIZE
    # on its own (confirmed directly: ~35 million executions across 16 parallel
    # workers, zero of them got past loadRom()'s size/ alignment rejection into
    # real GameBoy execution) - only load-rom has one right now
    # (tests/fuzz-seeds/load-rom/rom_only_16kb.gb); load-state has no committed
    # seed dir at all yet (EXISTS-guarded below), so that fuzzer currently only
    # ever exercises loadState()'s magic/version rejection path - a real
    # save-state seed would need to be regenerated whenever
    # gbemu::SAVE_STATE_VERSION bumps rather than committed as a static binary
    # blob, which is more machinery than this split needed on its own.
    #
    # The mkdir happens inside the test COMMAND itself (bash -c), not just once
    # at configure time - confirmed directly that relying on configure-time
    # creation alone is fragile: anything that clears tests/fuzz-corpus/ without
    # also re-running cmake (a cache miss included - actions/cache doesn't
    # create the restore path on a miss, it just leaves whatever's already
    # there) leaves ctest unable to find it. bash -c is safe to rely on
    # unconditionally here - this whole block is already Linux-only (see the
    # WIN32 FATAL_ERROR above).
    set(FUZZ_CORPUS_DIR ${CMAKE_SOURCE_DIR}/tests/fuzz-corpus)
    set(FUZZ_ARTIFACTS_DIR ${CMAKE_SOURCE_DIR}/tests/fuzz-artifacts)
    set(FUZZ_SEEDS_DIR ${CMAKE_SOURCE_DIR}/tests/fuzz-seeds)

    # Without an explicit -max_len, libFuzzer derives its own cap from
    # whatever's currently in the corpus - either the largest existing entry, or
    # a hardcoded 4096 bytes if the corpus is empty/small (confirmed directly
    # against both an empty and a partially-populated fuzz-corpus/). Either way
    # that's far below what real cartridges need: the mapper bank math (e.g.
    # src/mappers/mbc5.cpp's romSize()/ROM_BANK_SIZE) scales directly off the
    # actual ROM buffer size with no header-derived cap, and real hardware goes
    # up to 8MB/512 banks for MBC5 - MBC1's own bank-0x20/0x40/0x60 special case
    # alone needs a ROM past 512KB to even reach. 8388608 (0x800000) = MBC5's
    # real 8MB ceiling, so the fuzzer can reach every bank-count edge case any
    # supported mapper actually has, regardless of what the corpus happens to
    # contain on a given run. Save states aren't bank-math-bound the same way,
    # but there's no harm in reusing the same generous cap for load-state too.
    set(FUZZ_MAX_LEN 8388608)

    # -jobs/-workers run one fuzzing process per core in parallel, all pointed
    # at the same corpus directory - what actually lets them learn from each
    # other's finds (via -reload, on by default, periodically re-reading it)
    # rather than being fully isolated processes that happen to run at the same
    # time (confirmed directly: without a shared corpus dir, 16 parallel workers
    # each starting from nothing all got stuck at the exact same trivial
    # coverage level for the whole run). PROCESSORS tells ctest's own scheduler
    # each of these tests wants every core, so it doesn't also try to start
    # other tests alongside them and oversubscribe the machine - the two fuzz
    # targets' own tests still run one after another rather than concurrently
    # with each other, for the same reason.
    foreach(fuzz_name load-rom load-state)
      set(fuzz_corpus_dir ${FUZZ_CORPUS_DIR}/${fuzz_name})
      set(fuzz_artifacts_dir ${FUZZ_ARTIFACTS_DIR}/${fuzz_name})
      set(fuzz_seeds_dir ${FUZZ_SEEDS_DIR}/${fuzz_name})

      set(fuzz_corpus_args "'${fuzz_corpus_dir}/'")
      if(EXISTS ${fuzz_seeds_dir})
        string(APPEND fuzz_corpus_args " '${fuzz_seeds_dir}/'")
      endif()

      add_test(
        NAME "fuzz regression (${fuzz_name})"
        COMMAND
          bash -c
          "mkdir -p '${fuzz_corpus_dir}' '${fuzz_artifacts_dir}' && exec $<TARGET_FILE:gbemu-fuzz-${fuzz_name}> -max_total_time=60 -max_len=${FUZZ_MAX_LEN} -jobs=${NPROC} -workers=${NPROC} -artifact_prefix='${fuzz_artifacts_dir}/' ${fuzz_corpus_args}"
      )
      set_tests_properties("fuzz regression (${fuzz_name})"
                           PROPERTIES PROCESSORS ${NPROC})

      # Same as above but with no -max_total_time cap - a real, open-ended
      # fuzzing session instead of a shallow regression check, for manual/ local
      # use. Labeled "manual" and excluded from CI's own ctest invocation (-LE
      # manual, see ci.yml) - letting this run unbounded on every push would
      # make every CI run take as long as ctest's own default per-test timeout
      # (1500s) instead of the ~60s the regression check above costs. Still
      # bounded by that same default CTest timeout rather than truly infinite
      # either way - run explicitly via `ctest -R "fuzz \(unbounded\)"` for a
      # real, longer fuzzing session.
      add_test(
        NAME "fuzz (unbounded) (${fuzz_name})"
        COMMAND
          bash -c
          "mkdir -p '${fuzz_corpus_dir}' '${fuzz_artifacts_dir}' && exec $<TARGET_FILE:gbemu-fuzz-${fuzz_name}> -max_len=${FUZZ_MAX_LEN} -jobs=${NPROC} -workers=${NPROC} -artifact_prefix='${fuzz_artifacts_dir}/' ${fuzz_corpus_args}"
      )
      set_tests_properties("fuzz (unbounded) (${fuzz_name})"
                           PROPERTIES PROCESSORS ${NPROC} LABELS "manual")
    endforeach()
  endif()
endif()
