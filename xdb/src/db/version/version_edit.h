#ifndef STORAGE_XDB_DB_VERSION_VERSION_EDIT_H_
#define STORAGE_XDB_DB_VERSION_VERSION_EDIT_H_

#include <vector>
#include <set>

#include "db/format/internal_key.h"

namespace xdb {
class VersionSet;

struct FileMeta {
    FileMeta() : refs(0), file_size(0), allow_seeks(1 << 30) {}
    int refs;
    uint64_t number;
    uint64_t file_size;
    InternalKey smallest;
    InternalKey largest;
    int allow_seeks;
};

class VersionEdit {
 public:
    VersionEdit() : 
        log_number_(0),
        last_sequence_(0),
        next_file_number_(0),
        has_log_number_(false),
        has_last_sequence_(false),
        has_next_file_number_(false),
        has_comparator_name_(false) {}

    void SetLogNumber(uint64_t log_number) {
        has_log_number_ = true;
        log_number_ = log_number;
    }
    void SetLastSequence(SequenceNum last_sequence) {
        has_last_sequence_ = true;
        last_sequence_ = last_sequence;
    }
    void SetNextFileNumber(uint64_t next_file_number) {
        has_next_file_number_ = true;
        next_file_number_ = next_file_number;
    }
    void SetComparatorName(const Slice& name) {
        has_comparator_name_ = true;
        comparator_name_ = name.ToString();
    }
    void AddFile(int level, uint64_t number, uint64_t file_size
            ,const InternalKey& smallest, const InternalKey& largest) {
        FileMeta meta;
        meta.number = number;
        meta.file_size = file_size;
        meta.smallest = smallest;
        meta.largest = largest;
        new_files_.emplace_back(level, meta);
    }
    void DeleteFile(int level, uint64_t file_number) {
        delete_files_.emplace(level, file_number);
    }
    void SetCompactionPointer(int level, InternalKey key) {
        delete_files_.emplace(level, key);
    }
    void EncodeTo(std::string* dst);

    Status DecodeFrom(const Slice& src);
 private:
    friend class VersionSet;
    using DeleteSet = std::set<std::pair<int, uint64_t>>;

    std::vector<std::pair<int, FileMeta>> new_files_;
    DeleteSet delete_files_;
    std::vector<std::pair<int, InternalKey>> compaction_pointers_;
    uint64_t log_number_;
    SequenceNum last_sequence_;
    uint64_t next_file_number_;
    std::string comparator_name_;
    bool has_log_number_;
    bool has_last_sequence_;
    bool has_next_file_number_;
    bool has_comparator_name_;
};

};


#endif // STORAGE_XDB_DB_VERSION_VERSION_EDIT_H_