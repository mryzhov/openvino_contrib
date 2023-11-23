# Copyright (C) 2018-2023 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#
cmake_policy(SET CMP0057 NEW)

if(POLICY CMP0079)
  cmake_policy(SET CMP0079 NEW)
endif()


if(POLICY CMP0057)
  cmake_policy(SET CMP0057 NEW)
endif()

set(TARGET_NAME "user_ov_extensions")

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 11)
endif()

include(cmake/platforms.cmake)

find_package(OpenVINO REQUIRED COMPONENTS Runtime)
find_package(TBB COMPONENTS tbb tbbmalloc)
find_package(OpenCV COMPONENTS core)

set(OP_REQ_TBB "complex_mul" "fft")

set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include")

#
# Select specific operations
#

if(NOT CUSTOM_OPERATIONS)
  file(GLOB op_src "${SOURCES}/*.cpp")
  file(GLOB op_dirs LIST_DIRECTORIES true "${SOURCES}/*")
  list(REMOVE_ITEM op_dirs "${SOURCES}/cmake")

  foreach(op IN LISTS op_src)
    get_filename_component(op_name ${op} NAME_WE)
    list(APPEND CUSTOM_OPERATIONS ${op_name})
  endforeach()

  foreach(op_dir IN LISTS op_dirs)
    if(IS_DIRECTORY "${op_dir}")
      get_filename_component(op_name "${op_dir}" NAME)
      list(APPEND CUSTOM_OPERATIONS ${op_name})
    endif()
  endforeach()

  # remove .cpp file with all extensions
  list(REMOVE_ITEM CUSTOM_OPERATIONS ov_extension)
endif()

list(APPEND SRC "${SOURCES}/ov_extension.cpp")

# filter out some operations, requiring specific dependencies

if(NOT OpenCV_FOUND)
  list(REMOVE_ITEM SRC "${SOURCES}/fft.cpp")
  list(REMOVE_ITEM CUSTOM_OPERATIONS fft)
endif()

if(NOT TBB_FOUND)
  foreach(op IN LISTS OP_REQ_TBB)
    list(REMOVE_ITEM SRC "${SOURCES}/${op}.cpp")
    list(REMOVE_ITEM CUSTOM_OPERATIONS ${op})
  endforeach()
endif()

message("  List of custom operations in ${TARGET_NAME} extension: ")
foreach(op IN LISTS CUSTOM_OPERATIONS)
  if(IS_DIRECTORY "${SOURCES}/${op}")
    file(GLOB op_src "${SOURCES}/${op}/*.cpp")
    list(APPEND SRC ${op_src})
  elseif(EXISTS "${SOURCES}/${op}.cpp")
    list(APPEND SRC "${SOURCES}/${op}.cpp")
  else()
    message("${SOURCES}/${op}")
    message(FATAL_ERROR "${TARGET_NAME} does not have operation with name '${op}'")
  endif()

  message("    - ${op}")
endforeach()

#
# Create library
#

add_library(${TARGET_NAME} SHARED ${SRC})

set_target_properties(${TARGET_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin # .exe and .dll
  ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib # .lib and .a
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib # .so and .dylib
)

if(OpenCV_FOUND)
  target_link_libraries(${TARGET_NAME} PRIVATE ${OpenCV_LIBRARIES})
endif()

if(TBB_FOUND)
  target_link_libraries(${TARGET_NAME} PRIVATE TBB::tbb TBB::tbbmalloc)
endif()

# Left sentence_piece for backward compatibility
if("tokenizer" IN_LIST CUSTOM_OPERATIONS)
  add_subdirectory(${SOURCES}/tokenizer)
endif()

target_link_libraries(${TARGET_NAME} PRIVATE openvino::runtime)
#target_include_directories(${TARGET_NAME} PRIVATE $<TARGET_PROPERTY:openvino::runtime,INTERFACE_INCLUDE_DIRECTORIES>)

target_compile_definitions(${TARGET_NAME} PRIVATE ${CUSTOM_OPERATIONS})
target_include_directories(${TARGET_NAME} PUBLIC ./include/)

if(DEFINED SKBUILD)
  # Installing the extension module to the root of the package
  install(TARGETS ${TARGET_NAME} LIBRARY DESTINATION . )

  if(APPLE)
    set_target_properties(
      ${TARGET_NAME} PROPERTIES INSTALL_RPATH "@loader_path")
  else()
    set_target_properties(${TARGET_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN")
  endif()
endif()
