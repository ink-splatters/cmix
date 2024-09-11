# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(cmix VERSION 20 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast -march=native")

# Recursively find all source files for cmix
file(GLOB_RECURSE src
    "src/*.cpp"
    "src/*.h"
)
list (FILTER src EXCLUDE REGEX ".*enwik9-prepro.*" )


add_executable(cmix ${src})


# https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})


# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs support core irreader)


target_include_directories(cmix PRIVATE src)
target_link_libraries(cmix PRIVATE ${llvm_libs})
