cmake_minimum_required(VERSION 3.20)
project(llvm-utils VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include(AddLLVM)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

# Use the LLVM compiler flags to match the build
if(NOT LLVM_ENABLE_RTTI)
    add_compile_options(-fno-rtti)
endif()

if(NOT LLVM_ENABLE_EH)
    add_compile_options(-fno-exceptions)
endif()

# Compiler options
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Global output directory (all executables in <build>/bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Subdirectories
include_directories(include)
add_subdirectory(include)
add_subdirectory(lib)
add_subdirectory(src)

# Create the CMake package
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

install(EXPORT NiftyTargets
  FILE NiftyTargets.cmake
  NAMESPACE Nifty::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Nifty
)

configure_package_config_file(
  cmake/NiftyConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/NiftyConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Nifty
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/NiftyConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/NiftyConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/NiftyConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Nifty
)

# Enable file
include(cmake/GenerateEnableFile.cmake)
generate_enable_file(enable.in)
