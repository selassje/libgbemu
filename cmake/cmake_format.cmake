file(GLOB_RECURSE ALL_CMAKE_FILES CMakeLists.txt *.cmake)

list(FILTER ALL_CMAKE_FILES EXCLUDE REGEX "(builds)")

if("${ALL_CMAKE_FILES}" STREQUAL "")
  message(SEND_ERROR "No files to format")
endif()

find_program(CMAKE_FORMAT "cmake-format")

if(CMAKE_FORMAT)
  add_custom_target(cmake-format COMMAND ${CMAKE_FORMAT} -i ${ALL_CMAKE_FILES})
  add_custom_target(cmake-format-check COMMAND ${CMAKE_FORMAT} --check
                                               ${ALL_CMAKE_FILES})
else()
  message("cmake_format not available")
endif()
