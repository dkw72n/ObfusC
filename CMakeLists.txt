cmake_minimum_required(VERSION 3.13.4)
project(ObfusC)

# Set this to a valid LLVM installation dir
set(LT_LLVM_INSTALL_DIR "" CACHE PATH "/usr/bin/")

# Add the location of LLVMConfig.cmake to CMake search paths (so that
# find_package can locate it)
list(APPEND CMAKE_PREFIX_PATH "${LT_LLVM_INSTALL_DIR}/lib/llvm-19/cmake")

find_package(LLVM 19.1 REQUIRED CONFIG)

# Add the LLVM include paths and my includes
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
include_directories(SYSTEM ${CLANG_INCLUDE_DIRS})

set(CMAKE_CXX_STANDARD 20)

# LLVM is normally built without RTTI. Be consistent with that.
if(NOT LLVM_ENABLE_RTTI)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
  
add_definitions(-DOBFUSC_VERSION_MAJOR=0 -DOBFUSC_VERSION_MINOR=3 -DOBFUSC_VERSION_MICRO=0 -DOBFUSC_GIT_REV=${GIT_COMMIT_HASH})
  

  # Add Target and source dir 
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

add_library(ObfusC SHARED main.cpp)
target_include_directories(ObfusC PRIVATE "source")
target_include_directories(ObfusC PRIVATE "source/MBA")
target_include_directories(ObfusC PRIVATE "source/ISUB")
target_include_directories(ObfusC PRIVATE "source/CFF")
target_include_directories(ObfusC PRIVATE "source/BCF")
target_include_directories(ObfusC PRIVATE "source/IBR")
target_include_directories(ObfusC PRIVATE "source/SPLIT")
target_include_directories(ObfusC PRIVATE "source/ICALL")
target_include_directories(ObfusC PRIVATE "source/Passes")
add_subdirectory("source")
