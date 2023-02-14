#include "gtest/gtest.h"
#include "include/filter_policy.h"
#include "db/filter/filter_block.h"
#include "murmur3/MurmurHash3.h"
#include "util/coding.h"

namespace xdb {
    class TestPolicy : public FilterPolicy {
     public:
        const char* Name() const override {
            return "TestPolicy";
        }
        void CreatFilter(const Slice* keys, int n, std::string* dst) const override {
            for (int i = 0; i < n; i++) {
                uint32_t h = murmur3::MurmurHash3_x86_32(keys[i].data(), keys[i].size(), 0x789fed11);
                PutFixed32(dst, h);
            }
        }
        bool KeyMayMatch(const Slice& key, const Slice& filter) const override {
            uint32_t h = murmur3::MurmurHash3_x86_32(key.data(), key.size(), 0x789fed11);
            for (size_t i = 0; i + 4 <= filter.size(); i += 4) {
                if (h == DecodeFixed32(filter.data() + i)) {
                    return true;
                }
            }
            return false;
        }
    };
    TEST(FilterTest, SimpleTest) {
        FilterPolicy* policy = NewBloomFilterPolicy(15);
        FilterBlockBuilder builder(policy);
        builder.StartBlock(0);
        builder.AddKey("xiao");
        builder.AddKey("huang");
        builder.AddKey("tong");
        builder.AddKey("xue");
        Slice content = builder.Finish();
        FilterBlockReader reader(policy, content);
        ASSERT_TRUE(reader.KeyMayMatch(0,"xiao"));
        ASSERT_TRUE(reader.KeyMayMatch(0,"huang"));
        ASSERT_TRUE(reader.KeyMayMatch(0,"tong"));
        ASSERT_TRUE(reader.KeyMayMatch(0,"xue"));
        ASSERT_TRUE(reader.KeyMayMatch(100,"xiao"));
        ASSERT_TRUE(reader.KeyMayMatch(100,"huang"));
        ASSERT_TRUE(reader.KeyMayMatch(100,"tong"));
        ASSERT_TRUE(reader.KeyMayMatch(100,"xue"));
        delete policy;
    }

    TEST(FilterTest, MissingTest) {
        TestPolicy policy;
        FilterBlockBuilder builder(&policy);
        builder.StartBlock(0);
        builder.AddKey("xiao");
        builder.AddKey("huang");
        builder.AddKey("tong");
        builder.AddKey("xue");
        Slice content = builder.Finish();
        FilterBlockReader reader(&policy, content);
        ASSERT_TRUE(reader.KeyMayMatch(0,"xiao"));
        ASSERT_TRUE(reader.KeyMayMatch(0,"huang"));
        ASSERT_TRUE(reader.KeyMayMatch(0,"tong"));
        ASSERT_TRUE(reader.KeyMayMatch(0,"xue"));
        ASSERT_TRUE(!reader.KeyMayMatch(0,"da"));
        ASSERT_TRUE(!reader.KeyMayMatch(0,"huai"));
        ASSERT_TRUE(!reader.KeyMayMatch(0,"dan"));
        ASSERT_TRUE(!reader.KeyMayMatch(0,"qaq"));
    }

    TEST(FilterTest, DifferentOffset) {
        FilterPolicy* policy = NewBloomFilterPolicy(10000);
        FilterBlockBuilder builder(policy);
        builder.StartBlock(0);
        builder.AddKey("xiao");
        builder.AddKey("huang");
        builder.AddKey("tong");
        builder.AddKey("xue");
        builder.StartBlock(100);
        builder.AddKey("da");
        builder.AddKey("huai");
        builder.StartBlock(3000);
        builder.AddKey("dan");
        builder.StartBlock(6000);
        builder.AddKey("xiu");
        Slice content = builder.Finish();
        FilterBlockReader reader(policy, content);
        ASSERT_TRUE(reader.KeyMayMatch(0,"xiao"));
        ASSERT_TRUE(reader.KeyMayMatch(0,"huang"));
        ASSERT_TRUE(reader.KeyMayMatch(0,"tong"));
        ASSERT_TRUE(reader.KeyMayMatch(0,"xue"));
        ASSERT_TRUE(reader.KeyMayMatch(100,"xiao"));
        ASSERT_TRUE(reader.KeyMayMatch(100,"huang"));
        ASSERT_TRUE(reader.KeyMayMatch(100,"tong"));
        ASSERT_TRUE(reader.KeyMayMatch(100,"xue"));
        ASSERT_TRUE(reader.KeyMayMatch(0,"da"));
        ASSERT_TRUE(reader.KeyMayMatch(0,"huai"));
        ASSERT_TRUE(reader.KeyMayMatch(100,"da"));
        ASSERT_TRUE(reader.KeyMayMatch(100,"huai"));
        ASSERT_TRUE(!reader.KeyMayMatch(3000,"da"));
        ASSERT_TRUE(!reader.KeyMayMatch(3000,"huai"));
        ASSERT_TRUE(reader.KeyMayMatch(3000,"dan"));
        ASSERT_TRUE(reader.KeyMayMatch(6000,"xiu"));
        ASSERT_TRUE(!reader.KeyMayMatch(3000,"do"));
        ASSERT_TRUE(!reader.KeyMayMatch(6000,"int"));
    }
}