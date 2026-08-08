# Downloads the c-sp/game-boy-test-roms release zip (prebuilt ROMs for suites
# this repo doesn't already vendor directly, e.g. Mooneye, SameSuite, Mealybug,
# gambatte hwtests) into tests/game-boy-test-roms and extracts it there, once -
# CMAKE_CURRENT_SOURCE_DIR here is tests/ itself, since this file is include()-d
# from tests/CMakeLists.txt. Deliberately NOT under CMAKE_BINARY_DIR: this repo
# is configured from several different build dirs (one per preset -
# dev_ninja_gcc, dev_ninja_clang_linux, etc.), and a per-build-dir copy would
# mean downloading + extracting the same ~177MB/ 5000+ files again for every
# single one of them. Not committed to git either, for that same size reason -
# see CLAUDE.md. tests/.gitignore keeps this directory out of git status noise.
#
# EXPECTED_HASH pins the exact archive contents the same way Conan/emsdk already
# pin their own dependencies elsewhere in this build; bump both
# TEST_ROMS_VERSION and the hash together when moving to a newer release.

set(TEST_ROMS_VERSION "v7.0")
set(TEST_ROMS_URL
    "https://github.com/c-sp/game-boy-test-roms/releases/download/${TEST_ROMS_VERSION}/game-boy-test-roms-${TEST_ROMS_VERSION}.zip"
)
set(TEST_ROMS_SHA256
    "b9a9d7a1075aa35a3d07c07c34974048672d8520dca9e07a50178f5860c3832c")

set(TEST_ROMS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game-boy-test-roms")
set(TEST_ROMS_STAMP "${TEST_ROMS_DIR}/.extracted-${TEST_ROMS_VERSION}")

if(NOT EXISTS "${TEST_ROMS_STAMP}")
  message(STATUS "Fetching game-boy-test-roms ${TEST_ROMS_VERSION}")

  set(TEST_ROMS_ZIP "${CMAKE_CURRENT_BINARY_DIR}/game-boy-test-roms.zip")
  file(
    DOWNLOAD "${TEST_ROMS_URL}" "${TEST_ROMS_ZIP}"
    EXPECTED_HASH SHA256=${TEST_ROMS_SHA256}
    SHOW_PROGRESS)

  file(REMOVE_RECURSE "${TEST_ROMS_DIR}")
  file(MAKE_DIRECTORY "${TEST_ROMS_DIR}")
  file(ARCHIVE_EXTRACT INPUT "${TEST_ROMS_ZIP}" DESTINATION "${TEST_ROMS_DIR}")
  file(REMOVE "${TEST_ROMS_ZIP}")
  file(TOUCH "${TEST_ROMS_STAMP}")
endif()
