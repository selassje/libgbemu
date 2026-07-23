file(GLOB_RECURSE ALL_CMAKE_FILES CMakeLists.txt *.cmake)

list(FILTER ALL_CMAKE_FILES EXCLUDE REGEX "(builds)")

if("${ALL_CMAKE_FILES}" STREQUAL "")
  message(SEND_ERROR "No files to format")
endif()

find_program(CMAKE_FORMAT "cmake-format")

if(CMAKE_FORMAT)
  # cmake-format mishandles multi-file invocations: only the first file passed
  # in a single call is treated correctly (line-ending auto-detection and the
  # --check comparison silently break for every file after the first, which
  # combined with this repo's CRLF convention makes --check spuriously fail).
  # Run it once per file instead.
  set(CMAKE_FORMAT_COMMANDS)
  set(CMAKE_FORMAT_CHECK_COMMANDS)
  foreach(cmake_file ${ALL_CMAKE_FILES})
    list(
      APPEND
      CMAKE_FORMAT_COMMANDS
      COMMAND
      ${CMAKE_FORMAT}
      --line-ending
      auto
      -i
      "${cmake_file}")
    list(
      APPEND
      CMAKE_FORMAT_CHECK_COMMANDS
      COMMAND
      ${CMAKE_FORMAT}
      --line-ending
      auto
      --check
      "${cmake_file}")
  endforeach()

  add_custom_target(cmake-format ${CMAKE_FORMAT_COMMANDS})
  add_custom_target(cmake-format-check ${CMAKE_FORMAT_CHECK_COMMANDS})
else()
  message("cmake_format not available")
endif()
