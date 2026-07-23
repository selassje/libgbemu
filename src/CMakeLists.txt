set(MODULES ${CMAKE_CURRENT_LIST_DIR}/../import)

add_library(gb-core STATIC)

target_sources(
  gb-core
  PUBLIC FILE_SET
         public_modules
         TYPE
         CXX_MODULES
         BASE_DIRS
         ${MODULES}
         FILES
         ${MODULES}/gbemu.cppm)
