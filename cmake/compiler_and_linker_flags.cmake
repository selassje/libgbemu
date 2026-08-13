# Probed once at include time (not per-target) since check_ipo_supported()
# actually invokes the compiler/linker to verify - cheap once, wasteful repeated
# for every setup_lto() call below.
include(CheckIPOSupported)
check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_UNSUPPORTED_REASON)

# Only Release - the per-config INTERPROCEDURAL_OPTIMIZATION_RELEASE property,
# not the blunt CMAKE_BUILD_TYPE STREQUAL "Release" check every other
# Debug-vs-Release branch in this file uses, since that variable means nothing
# for a multi-config generator (release_vs_msvc/dev_vs_msvc pick their config at
# build time via --config, not at configure time).
function(setup_lto TARGET)
  if(LTO_SUPPORTED)
    set_target_properties(${TARGET}
                          PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
  else()
    message(STATUS "LTO not enabled for ${TARGET}: ${LTO_UNSUPPORTED_REASON}")
  endif()
endfunction()

function(setup_compiler_warnings TARGET)
  if(MSVC)
    target_compile_options(
      ${TARGET}
      PRIVATE /W4
              /WX
              /external:anglebrackets
              /external:W0
              /permissive-
              /wd4868
              /wd5045
              /wd4324
              /wd4530)
  else()
    target_compile_options(
      ${TARGET}
      PRIVATE -Wall
              -Wextra
              -Wpedantic
              -Werror
              -Wshadow
              -Wnon-virtual-dtor
              -Wold-style-cast
              -Wcast-align
              -Wunused
              -Woverloaded-virtual
              -Wconversion
              -Wsign-conversion
              -Wmisleading-indentation
              -Wnull-dereference
              -Wdouble-promotion
              -Wformat=2
              -Wimplicit-fallthrough
              -Wno-include-angled-in-module-purview
              -Wno-reserved-module-identifier
              -Wno-unknown-pragmas
              -Wno-c2y-extensions)

    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
      target_compile_options(
        ${TARGET}
        PRIVATE -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
                -Wno-unused-lambda-capture -Wno-unused-but-set-variable
                -Wno-unused-private-field)
    endif()

    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" AND NOT ${ENABLE_CLANG_TIDY})
      target_compile_options(
        ${TARGET} PRIVATE -Wduplicated-cond -Wduplicated-branches -Wlogical-op
                          -Wuseless-cast -Wnrvo)
    endif()
  endif()
endfunction()

function(setup_sanitizers TARGET)
  if(ENABLE_SANITIZERS)
    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
      target_compile_options(
        ${TARGET}
        PRIVATE -fsanitize=address -fsanitize=undefined
                -fno-sanitize-recover=undefined -fno-sanitize-merge
                -fno-omit-frame-pointer -fno-optimize-sibling-calls)
      target_link_options(${TARGET} PRIVATE -fsanitize=address
                          -fsanitize=undefined)
    endif()
    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
      target_compile_options(${TARGET} PRIVATE -fsanitize=address
                                               -Wno-conversion)
      target_link_options(${TARGET} PRIVATE -fsanitize=address)
    endif()
  endif()
endfunction()

function(setup_std_lib TARGET)
  if(ENABLE_LIBCXX AND ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    target_compile_options(${TARGET} PRIVATE -stdlib=libc++)
    target_link_options(${TARGET} PRIVATE -stdlib=libc++)
  endif()
endfunction()

function(setup_tests_flags TARGET)
  if(ENABLE_TESTS)
    target_compile_definitions(${TARGET} PRIVATE ENABLE_TESTS)
  endif()
endfunction()
