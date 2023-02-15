#ifndef STORAGE_XDB_DB_FORMAT_INTERNAL_KEY_H_
#define STORAGE_XDB_DB_FORMAT_INTERNAL_KEY_H_

#include <cassert>
#include <cstddef>

#include "include/slice.h"
#include "util/coding.h"
#include "include/comparator.h"
#include "include/status.h"

namespace xdb {

enum RecordType {
    KTypeDeletion = 0x0,
    KTypeInsertion = 0x1,
    KTypeLookup = 0x1
};

typedef uint64_t SequenceNum;

static const SequenceNum KMaxSequenceNum = (0x1ull << 56) - 1;
static uint64_t PackSequenceAndType(SequenceNum seq, RecordType type) {
    return (seq << 8) | type;
}

inline Slice ExtractUserKey(const Slice& internal_key) {
    assert(internal_key.size() >= 8);
    return Slice(internal_key.data(),internal_key.size() - 8);
}

class ParsedInternalKey {
 public:
    ParsedInternalKey() {}
    ParsedInternalKey(SequenceNum seq,const Slice& key, RecordType type) 
        : user_key_(key), seq_(seq), type_(type){}

    Slice user_key_;
    SequenceNum seq_;
    RecordType type_;
};

void AppendInternalKey(std::string* dst, const ParsedInternalKey& key);

// Key | Sequence 56bits | Type 8bits
class InternalKey {
 public:
    InternalKey() {}
    InternalKey(SequenceNum seq,const Slice& key, RecordType type) {
        AppendInternalKey(&rep_, ParsedInternalKey(seq,key,type));
    }
    ~InternalKey() = default;

    Slice user_key() const{
        return ExtractUserKey(rep_);
    }

    Slice Encode() const {
        return rep_;
    }

    void DecodeFrom(const Slice& s) {
        rep_.assign(s.data(),s.size());
    }

    void SetFrom(const ParsedInternalKey& key) {
        rep_.clear();
        AppendInternalKey(&rep_, key);
    }
 private:
    std::string rep_;
};


class InternalKeyComparator : public Comparator {
 public:
    explicit InternalKeyComparator(const Comparator* c) : user_cmp_(c) {}
    
    const char* Name() const override {
        return "xdb.InternalKeyComparator";
    }

    int Compare(const Slice& a, const Slice& b) const override;

    const Comparator* UserComparator() const { return user_cmp_; }

    int Compare(const InternalKey& a, const InternalKey& b) const {
        return Compare(a.Encode(), b.Encode());
    }

    void FindShortestMiddle(std::string* start, const Slice& limit) const override;

    void FindShortestBigger(std::string* start) const override;
 private:
    const Comparator* user_cmp_;
};

class LookupKey {
 public:
    LookupKey(const Slice& user_key, SequenceNum seq);
    
    ~LookupKey();

    LookupKey(const LookupKey&) = delete;
    LookupKey& operator=(const LookupKey&) = delete;

    Slice UserKey() const { return Slice(kstart_, end_ - kstart_ - 8); }
    
    Slice InternalKey() const { return Slice(kstart_, end_ - kstart_); }
    
    Slice FullKey() const { return Slice(start_, end_ - start_); }
 private:
    char* kstart_;
    char* end_;
    char* start_;
    char buf_[200];
};


}

#endif // STORAGE_XDB_DB_FORMAT_INTERNAL_KEY_H_