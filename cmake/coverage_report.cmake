# LLVM-only coverage support (source-based, via -fprofile-instr-generate),
# unlike aoc's version which also supports GNU/gcov+lcov. This project only ever
# builds with cl.exe or clang++ on Windows, never gcc, so the gcov path was
# dropped rather than carried over unused.

function(setup_coverage_report_target)
  if(ENABLE_COVERAGE_REPORT)
    message("Enabling Test Coverage Report")

    if(NOT ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
      message(
        SEND_ERROR
          "ENABLE_COVERAGE_REPORT requires Clang. Try disabling ENABLE_COVERAGE_REPORT"
      )
      return()
    endif()

    find_program(LLVM_PROFDATA "llvm-profdata" REQUIRED)
    find_program(LLVM_COV "llvm-cov" REQUIRED)

    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/coverage)

    get_property(
      TEST_TARGETS
      DIRECTORY ${CMAKE_SOURCE_DIR}/tests
      PROPERTY BUILDSYSTEM_TARGETS)
    foreach(TEST IN LISTS TEST_TARGETS)
      get_target_property(TARGET_TYPE ${TEST} TYPE)
      if(${TARGET_TYPE} STREQUAL "EXECUTABLE")
        set(OBJECTS ${OBJECTS} -object $<TARGET_FILE:${TEST}>)
      endif()
    endforeach()

    add_custom_target(
      generate-coverage-report
      COMMAND ${LLVM_PROFDATA} merge ${CMAKE_BINARY_DIR}/coverage/*.profraw
              --output ${CMAKE_BINARY_DIR}/coverage.profdata
      COMMAND
        ${LLVM_COV} show ${OBJECTS}
        -instr-profile=${CMAKE_BINARY_DIR}/coverage.profdata
        -ignore-filename-regex=tests -format=html
        -output-dir=${CMAKE_BINARY_DIR}/coverage-html
      COMMAND
        ${LLVM_COV} report ${OBJECTS}
        -instr-profile=${CMAKE_BINARY_DIR}/coverage.profdata
        -ignore-filename-regex=tests
      COMMAND
        ${LLVM_COV} export ${OBJECTS}
        -instr-profile=${CMAKE_BINARY_DIR}/coverage.profdata
        -ignore-filename-regex=tests -format=lcov >
        ${CMAKE_BINARY_DIR}/coverage.lcov
      COMMENT
        "HTML coverage report: ${CMAKE_BINARY_DIR}/coverage-html/index.html")
  endif()
endfunction()

function(setup_coverage_report_flags TARGET)
  if(ENABLE_COVERAGE_REPORT)
    target_compile_options(${TARGET} PRIVATE -fprofile-instr-generate
                                             -fcoverage-mapping)
    target_link_libraries(${TARGET} PRIVATE -fprofile-instr-generate
                                            -fcoverage-mapping)
  endif()
endfunction()
