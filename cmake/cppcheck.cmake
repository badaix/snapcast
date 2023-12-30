find_program(CPPCHECK_BINARY cppcheck NAMES cppcheck)

# If CppCheck executable found
if(CPPCHECK_BINARY)
  if(NOT DEFINED ENV{CPPCHECK_CACHE_DIR})
    file(REAL_PATH "~/.cache/cppcheck" cache_dir EXPAND_TILDE)
    set(ENV{CPPCHECK_CACHE_DIR} ${cache_dir})
  endif()

  set(CPPCHECK_CACHE_DIR
      $ENV{CPPCHECK_CACHE_DIR}
      CACHE STRING "cppcheck cache directory")

  # Check CppCheck version
  set(CPP_CHECK_CMD ${CPPCHECK_BINARY} --version)
  execute_process(
    COMMAND ${CPP_CHECK_CMD}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE CPP_CHECK_RESULT
    OUTPUT_VARIABLE CPP_CHECK_VERSION
    ERROR_VARIABLE CPP_CHECK_ERROR)

  include(ProcessorCount)
  ProcessorCount(CPU_CORES)

  # Check if version could be extracted
  if(CPP_CHECK_RESULT EQUAL 0)

    # Append desired arguments to CppCheck
    list(
      APPEND
      CPPCHECK_BINARY
      # Use all the available CPU cores
      "-j 4"
      # ${CPU_CORES}"
      "--std=c++17"
      "--enable=all"
      "--cppcheck-build-dir=${CPPCHECK_CACHE_DIR}"
      "--project=${CMAKE_BINARY_DIR}/compile_commands.json"
      "--error-exitcode=10"
      "--inline-suppr"
      "--suppress=unusedFunction"
      "--suppress=noExplicitConstructor"
      "--suppress=preprocessorErrorDirective")

    if(CPPCHECK_ARGS)
      set(CPPCHECK_ARGS_LIST ${CPPCHECK_ARGS})
      separate_arguments(CPPCHECK_ARGS_LIST)
      list(APPEND CPPCHECK_BINARY ${CPPCHECK_ARGS_LIST})
    endif()

    add_custom_command(
      OUTPUT create-cache-dir
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CPPCHECK_CACHE_DIR}
      COMMENT "Creating cppcheck cache dir: '${CPPCHECK_CACHE_DIR}'")

    add_custom_target(
      cppcheck
      COMMAND ${CPPCHECK_BINARY}
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Static code analysis using ${CPP_CHECK_VERSION}"
      DEPENDS create-cache-dir)
  endif()
endif()
