cmake_minimum_required(VERSION 3.15)
project(DupFinder VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${CMAKE_SOURCE_DIR}/third_party)
link_directories(${CMAKE_SOURCE_DIR}/third_party/xxHash)

find_library(XXHASH_LIB NAMES xxhash xxHash
    PATHS ${CMAKE_SOURCE_DIR}/third_party/xxHash
    NO_DEFAULT_PATH)

if(NOT XXHASH_LIB)
    message(FATAL_ERROR "xxHash library not found. Build third_party/xxHash or install libxxhash-dev")
else()
    message(STATUS "Found xxHash library: ${XXHASH_LIB}")
endif()

add_executable(find_dup_files
    ${CMAKE_SOURCE_DIR}/src/find_dup/main.cc
    ${CMAKE_SOURCE_DIR}/src/find_dup/search.cc)
target_include_directories(find_dup_files SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/xxHash)
target_link_directories(find_dup_files PRIVATE ${XXHASH_LIB})
target_link_libraries(find_dup_files PRIVATE ${XXHASH_LIB})


add_executable(delete_files_in_list ${CMAKE_SOURCE_DIR}/src/del_file_list/main.cc)
