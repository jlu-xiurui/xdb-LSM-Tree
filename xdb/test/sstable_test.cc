#include "gtest/gtest.h"
#include "include/sstable_builder.h"
#include "include/sstable_reader.h"
#include "util/file.h"
#include "include/env.h"
#include "include/iterator.h"

namespace xdb {
    TEST(ExampleTest, LinearIterater) {
        Option option;
        option.compress_type = KUnCompress;
        RandomReadFile* read_file;
        WritableFile* write_file;
        Env* env = DefaultEnv();
        std::string filename{"/home/xiurui/xdb/test_file"};
        Status s = env->NewWritableFile(filename, &write_file);
        ASSERT_TRUE(s.ok());
        SSTableBuilder builder(option, write_file);
        char buf[5];
        for (int i = 0; i < 10000; i++) {
            std::sprintf(buf, "%04d", i);
            builder.Add(Slice(buf, 4),"VAL");
        }
        s = builder.Finish();
        ASSERT_TRUE(s.ok());
        s = write_file->Sync();
        ASSERT_TRUE(s.ok());

        uint64_t file_size;
        s = env->NewRamdomReadFile(filename, &read_file);
        ASSERT_TRUE(s.ok());
        env->FileSize(filename, &file_size);
        SSTableReader* reader;
        s = SSTableReader::Open(option, read_file, file_size, &reader);
        ASSERT_TRUE(s.ok());
        ReadOption read_option;
        Iterator* iter = reader->NewIterator(read_option);
        iter->SeekToFirst();
        for (int i = 0; i < 10000; i++) {
            std::sprintf(buf, "%04d", i);
            Slice s = Slice(buf, 4);
            ASSERT_TRUE(iter->Valid());
            ASSERT_EQ(iter->Key().ToString(),s.ToString());
            ASSERT_EQ(iter->Value(),"VAL");
            iter->Next();
        }
        ASSERT_FALSE(iter->Valid());
        env->RemoveFile(filename);
        delete iter;
        delete reader;
        delete write_file;
    }

}