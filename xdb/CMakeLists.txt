cmake_minimum_required(VERSION 3.10)

set(CMAKE_BUILD_TYPE DEBUG)
# Complie as C++11
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_C_COMPILER "/usr/bin/clang")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")

project(xdb
        VERSION 2022.12
        LANGUAGES C CXX)

enable_testing()


# Test whether -Wthread-safety is available. See
# https://clang.llvm.org/docs/ThreadSafetyAnalysis.html
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wthread-safety HAVE_CLANG_THREAD_SAFETY)

# Includes.
set(XDB_SRC_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/src/include)
set(XDB_THIRD_PARTY_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/third_party)
set(XDB_CRC32C_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/third_party/crc32c/include)

add_library(xdb "")
target_sources(xdb
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/db/dbimpl.h"
        "${PROJECT_SOURCE_DIR}/src/db/dbimpl.cc"
        "${PROJECT_SOURCE_DIR}/src/db/option.cc"
        "${PROJECT_SOURCE_DIR}/src/db/filter/filter_block.cc"
        "${PROJECT_SOURCE_DIR}/src/db/filter/filter_block.h"
        "${PROJECT_SOURCE_DIR}/src/db/filter/bloom.cc"
        "${PROJECT_SOURCE_DIR}/src/db/format/internal_key.h"
        "${PROJECT_SOURCE_DIR}/src/db/format/internal_key.cc"
        "${PROJECT_SOURCE_DIR}/src/db/log/log_format.h"
        "${PROJECT_SOURCE_DIR}/src/db/log/log_reader.cc"
        "${PROJECT_SOURCE_DIR}/src/db/log/log_reader.h"
        "${PROJECT_SOURCE_DIR}/src/db/log/log_writer.cc"
        "${PROJECT_SOURCE_DIR}/src/db/log/log_writer.h"
        "${PROJECT_SOURCE_DIR}/src/db/memtable/skiplist.h"
        "${PROJECT_SOURCE_DIR}/src/db/memtable/membuffer.h"
        "${PROJECT_SOURCE_DIR}/src/db/memtable/memtable.h"
        "${PROJECT_SOURCE_DIR}/src/db/memtable/membuffer.cc"
        "${PROJECT_SOURCE_DIR}/src/db/memtable/memtable.cc"
        "${PROJECT_SOURCE_DIR}/src/db/sstable/block_builder.cc"
        "${PROJECT_SOURCE_DIR}/src/db/sstable/block_builder.h"
        "${PROJECT_SOURCE_DIR}/src/db/sstable/block_format.cc"
        "${PROJECT_SOURCE_DIR}/src/db/sstable/block_format.h"
        "${PROJECT_SOURCE_DIR}/src/db/sstable/block_reader.cc"
        "${PROJECT_SOURCE_DIR}/src/db/sstable/block_reader.h"
        "${PROJECT_SOURCE_DIR}/src/db/sstable/sstable_builder.cc"
        "${PROJECT_SOURCE_DIR}/src/db/sstable/sstable_reader.cc"
        "${PROJECT_SOURCE_DIR}/src/db/writebatch/writebatch.cc"
        "${PROJECT_SOURCE_DIR}/src/db/writebatch/writebatch_helper.h"
        "${PROJECT_SOURCE_DIR}/src/util/cache.cc"
        "${PROJECT_SOURCE_DIR}/src/util/coding.h"
        "${PROJECT_SOURCE_DIR}/src/util/coding.cc"
        "${PROJECT_SOURCE_DIR}/src/util/comparator.cc"
        "${PROJECT_SOURCE_DIR}/src/util/env.cc"
        "${PROJECT_SOURCE_DIR}/src/util/file.h"
        "${PROJECT_SOURCE_DIR}/src/util/filename.cc"
        "${PROJECT_SOURCE_DIR}/src/util/filename.h"
        "${PROJECT_SOURCE_DIR}/src/util/iterator.cc"
        "${PROJECT_SOURCE_DIR}/src/util/mutex.h"
        "${PROJECT_SOURCE_DIR}/src/util/status.cc"    
        "${PROJECT_SOURCE_DIR}/src/util/thread_annotations.h"  
    PUBLIC
        "${PROJECT_SOURCE_DIR}/src/include/cache.h"
        "${PROJECT_SOURCE_DIR}/src/include/comparator.h"
        "${PROJECT_SOURCE_DIR}/src/include/db.h"
        "${PROJECT_SOURCE_DIR}/src/include/env.h"
        "${PROJECT_SOURCE_DIR}/src/include/filter_policy.h"
        "${PROJECT_SOURCE_DIR}/src/include/iterator.h"
        "${PROJECT_SOURCE_DIR}/src/include/option.h"
        "${PROJECT_SOURCE_DIR}/src/include/slice.h"
        "${PROJECT_SOURCE_DIR}/src/include/sstable_builder.h"
        "${PROJECT_SOURCE_DIR}/src/include/sstable_reader.h"
        "${PROJECT_SOURCE_DIR}/src/include/status.h"
        "${PROJECT_SOURCE_DIR}/src/include/writebatch.h")

# include the murmur3 hash
add_library(murmur3 SHARED
    "${PROJECT_SOURCE_DIR}/third_party/murmur3/MurmurHash3.h"
    "${PROJECT_SOURCE_DIR}/third_party/murmur3/MurmurHash3.cc")
target_link_libraries(xdb murmur3)

# include the google/crc32c
add_subdirectory(third_party/crc32c)
target_link_libraries(xdb crc32c)

add_subdirectory(third_party/snappy)
target_link_libraries(xdb snappy)

add_subdirectory(third_party/googletest)
target_link_libraries(xdb gtest)

target_include_directories(xdb PUBLIC 
    ${PROJECT_SOURCE_DIR}/src ${XDB_THIRD_PARTY_INCLUDE_DIR} ${XDB_CRC32C_INCLUDE_DIR})

message(STATUS "CXX_COMPLIER is ${CMAKE_CXX_COMPILER_ID}")

if(HAVE_CLANG_THREAD_SAFETY)
    target_compile_options(xdb
        PUBLIC -Werror -Wthread-safety)
    message(STATUS "have CLANG_THREAD_SAFETY")
else(HAVE_CLANG_THREAD_SAFETY)
message(STATUS "don't have CLANG_THREAD_SAFETY")
endif(HAVE_CLANG_THREAD_SAFETY)

file(GLOB XDB_TEST_SOURCES "${PROJECT_SOURCE_DIR}/test/*test.cc")

# valgrind
find_program(VALGRIND_BIN valgrind)
if ("${VALGRIND_BIN}" STREQUAL "VALGRIND_BIN-NOTFOUND")
    message(WARNING "XDB/test couldn't find valgrind.")
else()
    message(STATUS "XDB/test found valgrind at ${VALGRIND_BIN}")
endif()

set(VALGRIND_OPTIONS
        --error-exitcode=1                      # if leaks are detected, return nonzero value
        # --gen-suppressions=all  # uncomment for leak suppression syntax
        --leak-check=full                       # detailed leak information
        --soname-synonyms=somalloc=*jemalloc*   # also intercept jemalloc
        --trace-children=yes                    # trace child processes
        --track-origins=yes                     # track origin of uninitialized values
        )

foreach (XDB_test_source ${XDB_TEST_SOURCES})
     # Create a human readable name.
    get_filename_component(XDB_test_filename ${XDB_test_source} NAME)
    string(REPLACE ".cc" "" XDB_test_name ${XDB_test_filename})
    
    # Add the test target separately and as part of "make check-tests".
    add_executable(${XDB_test_name} EXCLUDE_FROM_ALL ${XDB_test_source})
    
    target_link_libraries(${XDB_test_name} xdb gtest gmock_main)
    
    # Set test target properties and dependencies.
    set_target_properties(${XDB_test_name}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
        COMMAND ${XDB_test_name}
    )
    
        # Add the test under CTest.
    add_test(${XDB_test_name} ${VALGRIND_BIN} ${VALGRIND_OPTIONS}
        ${CMAKE_BINARY_DIR}/test/${XDB_test_name}
        --gtest_color=yes --gtest_output=xml:${CMAKE_BINARY_DIR}/test/unit_${test_name}.xml)
    add_test(${XDB_test_name} ${CMAKE_BINARY_DIR}/test/${XDB_test_name} --gtest_color=yes
                --gtest_output=xml:${CMAKE_BINARY_DIR}/test/${XDB_test_name}.xml)
endforeach(XDB_test_source ${XDB_TEST_SOURCES})