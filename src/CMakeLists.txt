set(MODULES ${CMAKE_CURRENT_LIST_DIR}/../import)

add_library(gbemu STATIC)

target_sources(
  gbemu
  PUBLIC FILE_SET
         public_modules
         TYPE
         CXX_MODULES
         BASE_DIRS
         ${MODULES}
         FILES
         ${MODULES}/gbemu.cppm)

setup_compiler_warnings(gbemu)
setup_clang_tidy(gbemu)
