#ifndef STORAGE_XDB_DB_FILTER_FILTER_BLOCK_H_
#define STORAGE_XDB_DB_FILTER_FILTER_BLOCK_H_

#include <vector>

#include "include/slice.h"
#include "include/filter_policy.h"

namespace xdb {

class FilterPolicy;

class FilterBlockBuilder {
 public:
    explicit FilterBlockBuilder(const FilterPolicy* policy);

    FilterBlockBuilder(const FilterBlockBuilder&) = delete;
    FilterBlockBuilder* operator=(const FilterBlockBuilder&) = delete;

    void StartBlock(uint64_t block_offset);
    void AddKey(const Slice& key);
    Slice Finish();
 private:
    void GenerateFilter();

    const FilterPolicy* policy_;
    std::string key_buffer_;
    std::vector<size_t> key_starts_;
    std::string result_;
    std::vector<uint32_t> filter_offsets_;
    std::vector<Slice> tmp_keys_;
};

class FilterBlockReader {
 public:
    FilterBlockReader(const FilterPolicy* policy, const Slice& contents);
    
    bool KeyMayMatch(uint64_t block_offset, const Slice& key);
 private:
    const FilterPolicy* policy_;
    const char* data_;
    const char* filter_offsets_start_;
    size_t filter_offsets_num_;
    size_t filter_block_size_length_;
};

}

#endif // STORAGE_XDB_DB_FILTER_FILTER_BLOCK_H_