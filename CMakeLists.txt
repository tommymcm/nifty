cmake_minimum_required(VERSION 3.20)
project(llvm-utils VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include(AddLLVM)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

# Compiler options
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
add_definitions(-fno-rtti)

# Global output directory (all executables in <build>/bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Subdirectories
include_directories(include)
add_subdirectory(include)
add_subdirectory(lib)
add_subdirectory(src)

configure_file(
  ${CMAKE_SOURCE_DIR}/enable.in
  ${CMAKE_BINARY_DIR}/enable
  @ONLY
)

install(
  FILES ${CMAKE_BINARY_DIR}/enable
  DESTINATION ${CMAKE_SOURCE_DIR}
)
