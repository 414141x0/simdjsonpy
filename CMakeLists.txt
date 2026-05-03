cmake_minimum_required(VERSION 3.21)
project(simdjsonpy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SIMDJSONPY_ENABLE_ASAN "Enable AddressSanitizer for simdjsonpy" OFF)
option(SIMDJSONPY_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for simdjsonpy" OFF)
option(SIMDJSONPY_ENABLE_TSAN "Enable ThreadSanitizer for simdjsonpy" OFF)

find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

add_subdirectory(ext/nanobind EXCLUDE_FROM_ALL)
add_subdirectory(ext/simdjson EXCLUDE_FROM_ALL)

set(SIMDJSONPY_MODULE_OPTIONS FREE_THREADED)
if (NOT SIMDJSONPY_ENABLE_ASAN AND NOT SIMDJSONPY_ENABLE_UBSAN AND NOT SIMDJSONPY_ENABLE_TSAN)
  list(APPEND SIMDJSONPY_MODULE_OPTIONS LTO)
endif()

nanobind_add_module(_core
    ${SIMDJSONPY_MODULE_OPTIONS}
    src/module.cpp
    src/bind_common.cpp
    src/bind_dom.cpp
    src/bind_ondemand.cpp
)

target_include_directories(_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(_core PRIVATE simdjson::simdjson)
target_compile_features(_core PRIVATE cxx_std_20)

if (SIMDJSONPY_ENABLE_ASAN)
  target_compile_options(_core PRIVATE -fsanitize=address -fno-omit-frame-pointer)
  target_link_options(_core PRIVATE -fsanitize=address)
  if (TARGET simdjson)
    target_compile_options(simdjson PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(simdjson PRIVATE -fsanitize=address)
  endif()
endif()

if (SIMDJSONPY_ENABLE_UBSAN)
  target_compile_options(_core PRIVATE -fsanitize=undefined -fno-omit-frame-pointer)
  target_link_options(_core PRIVATE -fsanitize=undefined)
  if (TARGET simdjson)
    target_compile_options(simdjson PRIVATE -fsanitize=undefined -fno-omit-frame-pointer)
    target_link_options(simdjson PRIVATE -fsanitize=undefined)
  endif()
endif()

if (SIMDJSONPY_ENABLE_TSAN)
  target_compile_options(_core PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
  target_link_options(_core PRIVATE -fsanitize=thread)
  if (TARGET simdjson)
    target_compile_options(simdjson PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(simdjson PRIVATE -fsanitize=thread)
  endif()
endif()

if (CMAKE_CONFIGURATION_TYPES)
  set(PYI_PREFIX $<CONFIG>/)
endif()

set_target_properties(_core PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/simdjsonpy"
)

nanobind_add_stub(
  simdjsonpy_stub
  MODULE simdjsonpy._core
  OUTPUT ${PYI_PREFIX}simdjsonpy/_core.pyi
  PYTHON_PATH ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS _core
)

install(TARGETS _core LIBRARY DESTINATION simdjsonpy)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/simdjsonpy/_core.pyi"
        DESTINATION simdjsonpy)
