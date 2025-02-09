if(CMAKE_VERSION VERSION_EQUAL "3.19.0" OR CMAKE_VERSION VERSION_GREATER
                                           "3.19.0")

  find_program(CLANG_TIDY run-clang-tidy NAMES run-clang-tidy)

  # If run-clang-tidy found
  if(CLANG_TIDY)

    include(ProcessorCount)
    ProcessorCount(CPU_CORES)

    add_custom_target(
      clang-tidy
      COMMAND ${CLANG_TIDY} -j ${CPU_CORES}
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Static code analysis using ${CLANG_TIDY}")

  endif()
endif()
