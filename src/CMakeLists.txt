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
         ${CMAKE_CURRENT_LIST_DIR}
         FILES
         ${MODULES}/gbemu.cppm
         ${CMAKE_CURRENT_LIST_DIR}/cpu.cppm
         ${CMAKE_CURRENT_LIST_DIR}/mmu.cppm)

target_sources(
  gbemu
  PRIVATE ${CMAKE_CURRENT_LIST_DIR}/gbemu.cpp ${CMAKE_CURRENT_LIST_DIR}/cpu.cpp
          ${CMAKE_CURRENT_LIST_DIR}/mmu.cpp)

# cl.exe already gets a working `std` module from CMake's own CXX_MODULE_STD=ON
# mechanism (Ninja and the Visual Studio generator both resolve it on their
# own). CMake's CXX_MODULE_STD explicitly refuses Clang+MSVC-STL ("Only libc++
# and libstdc++ are supported"), so clang needs std.ixx pointed out manually
# instead - same workaround aoc uses for MSVC, repurposed here for the toolchain
# CMake doesn't support at all.
if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows"
   AND ${CMAKE_GENERATOR} STREQUAL "Ninja"
   AND ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
  set(VC_TOOLS_INSTALL_DIR "$ENV{VcToolsInstallDir}")
  set(STD_MODULE_DIR ${VC_TOOLS_INSTALL_DIR}/modules)
  set(STD_MODULE ${STD_MODULE_DIR}/std.ixx)

  target_sources(
    gbemu
    PUBLIC FILE_SET
           std_module
           TYPE
           CXX_MODULES
           BASE_DIRS
           ${STD_MODULE_DIR}
           FILES
           ${STD_MODULE_DIR}/std.ixx)

  # It's MSVC's file, not ours - don't lint it.
  set_source_files_properties(${STD_MODULE_DIR}/std.ixx PROPERTIES SKIP_LINTING
                                                                   ON)
endif()

setup_compiler_warnings(gbemu)
setup_clang_tidy(gbemu)
setup_msvc_analysis(gbemu)
setup_coverage_report_flags(gbemu)
setup_std_lib(gbemu)
