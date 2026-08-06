set(MODULES ${CMAKE_CURRENT_LIST_DIR}/../import)

add_library(libgbemu STATIC)

target_sources(
  libgbemu
  PUBLIC FILE_SET
         public_modules
         TYPE
         CXX_MODULES
         BASE_DIRS
         ${MODULES}
         ${CMAKE_CURRENT_LIST_DIR}
         FILES
         ${MODULES}/gbemu.cppm
         ${CMAKE_CURRENT_LIST_DIR}/apu.cppm
         ${CMAKE_CURRENT_LIST_DIR}/cpu.cppm
         ${CMAKE_CURRENT_LIST_DIR}/mmu.cppm
         ${CMAKE_CURRENT_LIST_DIR}/mapper.cppm
         ${CMAKE_CURRENT_LIST_DIR}/ppu.cppm
         ${CMAKE_CURRENT_LIST_DIR}/boot_rom.cppm
         ${CMAKE_CURRENT_LIST_DIR}/regs.cppm
         ${CMAKE_CURRENT_LIST_DIR}/hardware_mode.cppm
         ${CMAKE_CURRENT_LIST_DIR}/serialization.cppm)

if(MSVC)
  # MSBuild's real build infers /interface per module-interface file from
  # FILE_SET CXX_MODULES, but VS IntelliSense's own design-time build
  # reconstruction drops it, misdiagnosing every `export` here as C3378 ("can be
  # exported only from a module interface unit"). Force it explicitly so
  # IntelliSense's command line matches the real one.
  set_source_files_properties(
    ${MODULES}/gbemu.cppm
    ${CMAKE_CURRENT_LIST_DIR}/apu.cppm
    ${CMAKE_CURRENT_LIST_DIR}/cpu.cppm
    ${CMAKE_CURRENT_LIST_DIR}/mmu.cppm
    ${CMAKE_CURRENT_LIST_DIR}/mapper.cppm
    ${CMAKE_CURRENT_LIST_DIR}/ppu.cppm
    ${CMAKE_CURRENT_LIST_DIR}/boot_rom.cppm
    ${CMAKE_CURRENT_LIST_DIR}/regs.cppm
    ${CMAKE_CURRENT_LIST_DIR}/hardware_mode.cppm
    ${CMAKE_CURRENT_LIST_DIR}/serialization.cppm
    PROPERTIES COMPILE_OPTIONS /interface)
endif()

target_sources(
  libgbemu
  PRIVATE ${CMAKE_CURRENT_LIST_DIR}/gbemu.cpp
          ${CMAKE_CURRENT_LIST_DIR}/apu.cpp
          ${CMAKE_CURRENT_LIST_DIR}/cpu.cpp
          ${CMAKE_CURRENT_LIST_DIR}/mmu.cpp
          ${CMAKE_CURRENT_LIST_DIR}/mapper.cpp
          ${CMAKE_CURRENT_LIST_DIR}/ppu.cpp
          ${CMAKE_CURRENT_LIST_DIR}/boot_rom.cpp
          ${CMAKE_CURRENT_LIST_DIR}/serialization.cpp)

# cl.exe already gets a working `std` module from CMake's own CXX_MODULE_STD=ON
# mechanism (Ninja and the Visual Studio generator both resolve it on their
# own). CMake's CXX_MODULE_STD explicitly refuses Clang+MSVC-STL ("Only libc++
# and libstdc++ are supported"), so clang needs std.ixx pointed out manually
# instead - same workaround aoc uses for MSVC, repurposed here for the toolchain
# CMake doesn't support at all. Excludes Emscripten: emcc also reports
# CMAKE_CXX_COMPILER_ID "Clang" (it's a clang wrapper), but on a Windows host
# targeting wasm there's no MSVC STL involved at all - the Emscripten toolchain
# provides its own std module via
# CMAKE_CXX_MODULE_STD/CMAKE_EXPERIMENTAL_CXX_IMPORT_STD instead, and
# $ENV{VcToolsInstallDir} is unset outside an MSVC dev shell, which without this
# guard resolved STD_MODULE_DIR to a bare "/modules" and failed to find
# "/modules/std.ixx".
if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows"
   AND ${CMAKE_GENERATOR} STREQUAL "Ninja"
   AND ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang"
   AND NOT EMSCRIPTEN)
  set(VC_TOOLS_INSTALL_DIR "$ENV{VcToolsInstallDir}")
  set(STD_MODULE_DIR ${VC_TOOLS_INSTALL_DIR}/modules)
  set(STD_MODULE ${STD_MODULE_DIR}/std.ixx)

  target_sources(
    libgbemu
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

setup_compiler_warnings(libgbemu)
setup_clang_tidy(libgbemu)
setup_msvc_analysis(libgbemu)
setup_coverage_report_flags(libgbemu)
setup_sanitizers(libgbemu)
setup_std_lib(libgbemu)
setup_tests_flags(libgbemu)
