# CMake generated Testfile for 
# Source directory: /home/xiurui/xdb-LSM-Tree/xdb
# Build directory: /home/xiurui/xdb-LSM-Tree/xdb/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(block_test "/home/xiurui/xdb-LSM-Tree/xdb/build/test/block_test" "--gtest_color=yes" "--gtest_output=xml:/home/xiurui/xdb-LSM-Tree/xdb/build/test/block_test.xml")
set_tests_properties(block_test PROPERTIES  _BACKTRACE_TRIPLES "/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;159;add_test;/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;0;")
add_test(cache_test "/home/xiurui/xdb-LSM-Tree/xdb/build/test/cache_test" "--gtest_color=yes" "--gtest_output=xml:/home/xiurui/xdb-LSM-Tree/xdb/build/test/cache_test.xml")
set_tests_properties(cache_test PROPERTIES  _BACKTRACE_TRIPLES "/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;159;add_test;/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;0;")
add_test(coding_test "/home/xiurui/xdb-LSM-Tree/xdb/build/test/coding_test" "--gtest_color=yes" "--gtest_output=xml:/home/xiurui/xdb-LSM-Tree/xdb/build/test/coding_test.xml")
set_tests_properties(coding_test PROPERTIES  _BACKTRACE_TRIPLES "/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;159;add_test;/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;0;")
add_test(db_test "/home/xiurui/xdb-LSM-Tree/xdb/build/test/db_test" "--gtest_color=yes" "--gtest_output=xml:/home/xiurui/xdb-LSM-Tree/xdb/build/test/db_test.xml")
set_tests_properties(db_test PROPERTIES  _BACKTRACE_TRIPLES "/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;159;add_test;/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;0;")
add_test(env_test "/home/xiurui/xdb-LSM-Tree/xdb/build/test/env_test" "--gtest_color=yes" "--gtest_output=xml:/home/xiurui/xdb-LSM-Tree/xdb/build/test/env_test.xml")
set_tests_properties(env_test PROPERTIES  _BACKTRACE_TRIPLES "/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;159;add_test;/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;0;")
add_test(example_test "/home/xiurui/xdb-LSM-Tree/xdb/build/test/example_test" "--gtest_color=yes" "--gtest_output=xml:/home/xiurui/xdb-LSM-Tree/xdb/build/test/example_test.xml")
set_tests_properties(example_test PROPERTIES  _BACKTRACE_TRIPLES "/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;159;add_test;/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;0;")
add_test(filter_block_test "/home/xiurui/xdb-LSM-Tree/xdb/build/test/filter_block_test" "--gtest_color=yes" "--gtest_output=xml:/home/xiurui/xdb-LSM-Tree/xdb/build/test/filter_block_test.xml")
set_tests_properties(filter_block_test PROPERTIES  _BACKTRACE_TRIPLES "/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;159;add_test;/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;0;")
add_test(memtable_test "/home/xiurui/xdb-LSM-Tree/xdb/build/test/memtable_test" "--gtest_color=yes" "--gtest_output=xml:/home/xiurui/xdb-LSM-Tree/xdb/build/test/memtable_test.xml")
set_tests_properties(memtable_test PROPERTIES  _BACKTRACE_TRIPLES "/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;159;add_test;/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;0;")
add_test(sstable_test "/home/xiurui/xdb-LSM-Tree/xdb/build/test/sstable_test" "--gtest_color=yes" "--gtest_output=xml:/home/xiurui/xdb-LSM-Tree/xdb/build/test/sstable_test.xml")
set_tests_properties(sstable_test PROPERTIES  _BACKTRACE_TRIPLES "/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;159;add_test;/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;0;")
add_test(sstable_write_test "/home/xiurui/xdb-LSM-Tree/xdb/build/test/sstable_write_test" "--gtest_color=yes" "--gtest_output=xml:/home/xiurui/xdb-LSM-Tree/xdb/build/test/sstable_write_test.xml")
set_tests_properties(sstable_write_test PROPERTIES  _BACKTRACE_TRIPLES "/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;159;add_test;/home/xiurui/xdb-LSM-Tree/xdb/CMakeLists.txt;0;")
subdirs("third_party/crc32c")
subdirs("third_party/snappy")
subdirs("third_party/googletest")
