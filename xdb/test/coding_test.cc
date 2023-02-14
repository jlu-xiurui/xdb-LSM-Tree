#include "util/coding.h"
#include "gtest/gtest.h"

namespace xdb {

TEST(CodingTest, Fixed32) {
    std::string str;
    for (uint32_t i = 0; i < 1000; i++){
        PutFixed32(&str,i);
    }
    const char* data = str.data();
    for (uint32_t i = 0; i < 1000; i++){
        uint32_t val = DecodeFixed32(data);
        ASSERT_EQ(i, val);
        data += sizeof(uint32_t);
    }
}

TEST(CodingTest, Fixed64) {
    std::string str;
    for (uint64_t i = 0; i < 1000; i++){
        PutFixed64(&str,i);
    }
    const char* data = str.data();
    for (uint64_t i = 0; i < 1000; i++){
        uint64_t val = DecodeFixed64(data);
        ASSERT_EQ(i, val);
        data += sizeof(uint64_t);
    }
}

TEST(CodingTest, Varint32) {
    std::string str;
    for (uint32_t i = 0; i < (1 << 16); i++){
        PutVarint32(&str, i);
    }
    Slice s(str);
    for (uint32_t i = 0; i < (1 << 16); i++){
        uint32_t val;
        bool done = GetVarint32(&s,&val);
        ASSERT_EQ(i, val);
        ASSERT_EQ(done, true);
    }
     for (uint32_t i = 0; i < 100; i++){
        uint32_t val;
        bool done = GetVarint32(&s,&val);
        ASSERT_EQ(done, false);
    }
}

TEST(CodingTest, Varint64) {
    std::string str;
    for (uint64_t i = 0; i < (1 << 16); i++){
        PutVarint64(&str, i);
    }
    Slice s(str);
    for (uint64_t i = 0; i < (1 << 16); i++){
        uint64_t val;
        bool done = GetVarint64(&s,&val);
        ASSERT_EQ(i, val);
        ASSERT_EQ(done, true);
    }
     for (uint32_t i = 0; i < 100; i++){
        uint64_t val;
        bool done = GetVarint64(&s,&val);
        ASSERT_EQ(done, false);
    }
}

TEST(CodingTest, Varint32Overflow) {
    Slice s("\x81\x82\x83\x84\x85");
    uint32_t val;
    ASSERT_EQ(DecodeVarint32(s.data(), s.data() + s.size(),&val), nullptr);
}

TEST(CodingTest, LengthPrefixed) {
    std::string str;
    PutLengthPrefixedSlice(&str,Slice("huangxuan"));
    PutLengthPrefixedSlice(&str,Slice("love"));
    PutLengthPrefixedSlice(&str,Slice("xiurui"));
    PutLengthPrefixedSlice(&str,Slice("qaq"));

    Slice slice(str);
    Slice result;
    ASSERT_TRUE(GetLengthPrefixedSlice(&slice, &result));
    ASSERT_EQ("huangxuan", result.ToString());
    ASSERT_TRUE(GetLengthPrefixedSlice(&slice, &result));
    ASSERT_EQ("love", result.ToString());
    ASSERT_TRUE(GetLengthPrefixedSlice(&slice, &result));
    ASSERT_EQ("xiurui", result.ToString());
    ASSERT_TRUE(GetLengthPrefixedSlice(&slice, &result));
    ASSERT_EQ("qaq", result.ToString());
    ASSERT_FALSE(GetLengthPrefixedSlice(&slice, &result));
}
}