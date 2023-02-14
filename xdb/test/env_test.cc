#include "gtest/gtest.h"
#include "util/file.h"
#include "include/env.h"

namespace xdb {
    TEST(ExampleTest, WriteAndRead) {
        // Write file
        RandomReadFile* read_file;
        WritableFile* write_file;
        Env* env = DefaultEnv();
        std::string filename{"/home/xiurui/xdb/test_file"};
        Status s = env->NewWritableFile(filename, &write_file);
        ASSERT_TRUE(s.ok());
        std::string magic = "123456124365";
        s = write_file->Append(magic);
        ASSERT_TRUE(s.ok());
        s = write_file->Sync();
        ASSERT_TRUE(s.ok());
        uint64_t file_size;
        // read file and check
        s = env->NewRamdomReadFile(filename, &read_file);
        ASSERT_TRUE(s.ok());
        s = env->FileSize(filename, &file_size);
        ASSERT_TRUE(s.ok());
        env->RemoveFile(filename);
        Slice result;
        char buf[50];
        s = read_file->Read(0,12,&result,buf);
        ASSERT_TRUE(s.ok());
        ASSERT_EQ(magic, result);
        delete read_file;
        delete write_file;
    }

}