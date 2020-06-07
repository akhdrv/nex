cmake_minimum_required(VERSION 3.16)

project(native_example)
set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fPIC")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ./lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ./lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ./bin)

file(GLOB SOURCE_FILES "*.cpp" )

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

include_directories(../../src)
include_directories(../../deps/node/src)
include_directories(../../deps/uvw)
include_directories(../../deps/picohttpparser)
include_directories(../../deps/node/deps/v8/include)
include_directories(../../deps/node/deps/uv/include)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".so")