if(ENABLE_CLANG_TIDY)
  message("Clang-tidy enabled")
  find_program(CLANG_TIDY_EXE NAMES "clang-tidy" REQUIRED)
endif()
function(setup_clang_tidy TARGET)
  if(ENABLE_CLANG_TIDY)
    set_target_properties(${TARGET} PROPERTIES CXX_CLANG_TIDY
                                               "${CLANG_TIDY_EXE}")
  endif()
endfunction(setup_clang_tidy TARGET)

function(setup_msvc_analysis TARGET)
  if(ENABLE_MSVC_ANALYSIS)
    set(RULESET_FILE ${CMAKE_SOURCE_DIR}/.vsAnalyze.ruleset)
    get_filename_component(CXX_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    set(PLUGIN ${CXX_DIR}/EspXEngine.dll)
    target_compile_definitions(${TARGET} PRIVATE CODE_ANALYSIS)
    target_compile_options(
      ${TARGET} PRIVATE /analyze /analyze:ruleset${RULESET_FILE}
                        /analyze:plugin${PLUGIN} /analyze:external-)
  endif()
endfunction(setup_msvc_analysis)
