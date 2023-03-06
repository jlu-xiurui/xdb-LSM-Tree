#include "gtest/gtest.h"
#include "include/db.h"
#include <random>
#include <iostream>
#include "crc32c/crc32c.h"

namespace xdb {
    TEST(ExampleTest, SSTableTest) {
        Option option;
        WriteOption write_option;
        DB* db;
        DestoryDB(option, "/home/xiurui/test");
        Status s = DB::Open(option,"/home/xiurui/test",&db);
        ASSERT_TRUE(s.ok());
        for (int i = 0; i < 10000; i++) {
            s = db->Put(write_option, std::to_string(i), std::to_string(i));
            ASSERT_TRUE(s.ok());
        }
        delete db;
        db = nullptr;
        s = DB::Open(option,"/home/xiurui/test",&db);
        ASSERT_TRUE(s.ok());
    }
}