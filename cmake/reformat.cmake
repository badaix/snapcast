find_program(CLANG_FORMAT "clang-format")
if(CLANG_FORMAT)
  file(GLOB_RECURSE CHECK_CXX_SOURCE_FILES common/*.[ch]pp client/*.[ch]pp
       server/*.[ch]pp)

  list(REMOVE_ITEM CHECK_CXX_SOURCE_FILES "${CMAKE_SOURCE_DIR}/common/json.hpp")

  add_custom_target(
    reformat-source
    COMMAND ${CLANG_FORMAT} -i -style=file ${CHECK_CXX_SOURCE_FILES}
    COMMENT "Auto formatting of all source files")
endif()

find_program(CMAKE_FORMAT "cmake-format")
if(CMAKE_FORMAT)
  file(GLOB_RECURSE CHECK_CMAKE_SOURCE_FILES CMakeLists.txt *.cmake)

  add_custom_target(
    reformat-cmake
    COMMAND ${CMAKE_FORMAT} -i ${CHECK_CMAKE_SOURCE_FILES}
    COMMENT "Auto formatting of all CMakeLists.txt files")
endif()

find_program(AUTOPEP "autopep8")
if(AUTOPEP)
  file(GLOB_RECURSE CHECK_PYTHON_SOURCE_FILES *.py)

  add_custom_target(
    reformat-python
    COMMAND ${AUTOPEP} -i ${CHECK_PYTHON_SOURCE_FILES}
    COMMENT "Auto formatting of all Python files")
endif()

if(CLANG_FORMAT
   AND CMAKE_FORMAT
   AND AUTOPEP)
  add_custom_target(
    reformat
    DEPENDS reformat-cmake reformat-source reformat-python
    COMMENT "Auto formatting of all source and CMakeLists.txt files")
endif()
