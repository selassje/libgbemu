# Self-bootstraps Conan when the toolchain file a preset points at doesn't exist
# yet (fresh clone, or `builds/` was deleted/cleaned). Must be include()-d
# BEFORE project(), since CMAKE_TOOLCHAIN_FILE is only consulted there.

if(DEFINED CMAKE_TOOLCHAIN_FILE AND NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
  message(
    STATUS
      "Conan toolchain not found at ${CMAKE_TOOLCHAIN_FILE} - bootstrapping via 'conan install'"
  )

  get_filename_component(CONAN_OUTPUT_DIR "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
  get_filename_component(CMAKE_BIN_DIR "${CMAKE_COMMAND}" DIRECTORY)

  set(CONAN_INSTALL_ARGS
      --build=missing
      -s
      build_type=Debug
      -of
      "${CONAN_OUTPUT_DIR}"
      # We drive CMake ourselves via our own explicit CMakePresets.json; without
      # this, CMakeToolchain also writes a "conan-debug" preset into the shared
      # root CMakeUserPresets.json on every install, which clutters CMake Tools'
      # preset picker and collides when multiple presets/output folders write to
      # that same shared file in one session.
      -c
      tools.cmake.cmaketoolchain:user_presets=)

  # Ninja can't auto-discover cl.exe the way the Visual Studio generator can, so
  # Conan's CMakeToolchain needs to be told which generator it's for.
  if(CMAKE_GENERATOR STREQUAL "Ninja")
    list(APPEND CONAN_INSTALL_ARGS -c
         tools.cmake.cmaketoolchain:generator=Ninja)
  endif()

  # A preset that wants clang sets CXX/CC in its "environment" block as a hint
  # for us to read here; the actual compiler selection then comes from this conf
  # (see architecture memory: clang links fine against the same MSVC-built
  # Catch2, no separate binary needed).
  if(DEFINED ENV{CXX} AND "$ENV{CXX}" MATCHES "clang")
    list(
      APPEND
      CONAN_INSTALL_ARGS
      -c
      "tools.build:compiler_executables={\"c\": \"clang\", \"cpp\": \"clang++\"}"
    )
  endif()

  # ENABLE_LIBCXX presets build our own code against libc++ (-stdlib=libc++);
  # Catch2 must be built against the same stdlib or linking fails with an ABI
  # mismatch against the default profile's libstdc++11 build. libc++ isn't a
  # valid settings.compiler.libcxx value for compiler=gcc (the default profile's
  # detected compiler), so compiler/compiler.version must be overridden to clang
  # too, not just libcxx.
  if(ENABLE_LIBCXX)
    execute_process(COMMAND clang++ --version
                    OUTPUT_VARIABLE CLANG_VERSION_OUTPUT)
    string(REGEX MATCH "version ([0-9]+)" CLANG_VERSION_MATCH
                 "${CLANG_VERSION_OUTPUT}")
    list(
      APPEND
      CONAN_INSTALL_ARGS
      -s
      compiler=clang
      -s
      compiler.version=${CMAKE_MATCH_1}
      -s
      compiler.libcxx=libc++
      -s
      compiler.cppstd=gnu17)
  endif()

  # Conan may need to build a dependency (e.g. Catch2) from source, which shells
  # out to cmake itself; make sure it can find this same cmake even though it's
  # deliberately not on the persistent PATH.
  if(CMAKE_HOST_WIN32)
    set(PATH_LIST_SEP ";")
  else()
    set(PATH_LIST_SEP ":")
  endif()
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -E env
      "PATH=${CMAKE_BIN_DIR}${PATH_LIST_SEP}$ENV{PATH}" conan install
      "${CMAKE_SOURCE_DIR}" ${CONAN_INSTALL_ARGS}
    RESULT_VARIABLE CONAN_INSTALL_RESULT)

  if(NOT CONAN_INSTALL_RESULT EQUAL 0)
    message(FATAL_ERROR "conan install failed (exit ${CONAN_INSTALL_RESULT}); "
                        "toolchain file still missing: ${CMAKE_TOOLCHAIN_FILE}")
  endif()

  if(NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
    message(FATAL_ERROR "conan install ran but toolchain file still missing: "
                        "${CMAKE_TOOLCHAIN_FILE}")
  endif()
endif()
