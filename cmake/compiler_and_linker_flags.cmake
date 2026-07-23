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
              )

    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
      target_compile_options(
        ${TARGET} PRIVATE -Wno-unused-function
                          -Wno-unused-variable
                          -Wno-unused-parameter
                          -Wno-unused-lambda-capture
                          -Wno-unused-but-set-variable)
    endif()

    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" AND NOT ${ENABLE_CLANG_TIDY})
      target_compile_options(
        ${TARGET} PRIVATE -Wduplicated-cond -Wduplicated-branches -Wlogical-op
                          -Wuseless-cast -Wnrvo)
    endif()
  endif()
endfunction()
