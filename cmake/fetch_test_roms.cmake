# Downloads the c-sp/game-boy-test-roms release zip (prebuilt ROMs for suites
# this repo doesn't already vendor directly, e.g. Mooneye, SameSuite,
# Mealybug, gambatte hwtests) into the build tree and extracts it there, once.
# Not committed to git: the release is ~177MB across 5000+ files, versus the
# few small suites (gb-test-roms, dmg-acid2, cgb-acid2) that are vendored
# directly under tests/ - see CLAUDE.md for the size rationale.
#
# EXPECTED_HASH pins the exact archive contents the same way Conan/emsdk
# already pin their own dependencies elsewhere in this build; bump both
# TEST_ROMS_VERSION and the hash together when moving to a newer release.

set(TEST_ROMS_VERSION "v7.0")
set(TEST_ROMS_URL
    "https://github.com/c-sp/game-boy-test-roms/releases/download/${TEST_ROMS_VERSION}/game-boy-test-roms-${TEST_ROMS_VERSION}.zip"
)
set(TEST_ROMS_SHA256
    "b9a9d7a1075aa35a3d07c07c34974048672d8520dca9e07a50178f5860c3832c")

set(TEST_ROMS_DIR "${CMAKE_BINARY_DIR}/game-boy-test-roms")
set(TEST_ROMS_STAMP "${TEST_ROMS_DIR}/.extracted-${TEST_ROMS_VERSION}")

if(NOT EXISTS "${TEST_ROMS_STAMP}")
  message(STATUS "Fetching game-boy-test-roms ${TEST_ROMS_VERSION}")

  set(TEST_ROMS_ZIP "${CMAKE_BINARY_DIR}/game-boy-test-roms.zip")
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
