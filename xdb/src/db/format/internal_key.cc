#include "db/format/internal_key.h"
namespace xdb {

void AppendInternalKey(std::string* dst, const ParsedInternalKey& key) {
    dst->append(key.user_key_.data(),key.user_key_.size());;
    PutFixed64(dst, PackSequenceAndType(key.seq_,key.type_));
}

LookupKey::LookupKey(const Slice& user_key, SequenceNum seq) {
    size_t key_size = user_key.size();
    size_t need = key_size + 13;
    if (need < sizeof(buf_)) {
        start_ = buf_;
    } else {
        start_ = new char[need];
    }
    kstart_ = EncodeVarint32(start_, key_size + 8);
    std::memcpy(kstart_, user_key.data(), key_size);
    EncodeFixed64(kstart_ + key_size, (seq << 8) | KTypeLookup);
    end_ = kstart_ + key_size + 8;
}

LookupKey::~LookupKey() {
    if(start_ != buf_) {
        delete[] start_;
    }
}

int InternalKeyComparator::Compare(const Slice& a, const Slice& b) const {
    int r = user_cmp_->Compare(ExtractUserKey(a), ExtractUserKey(b));
    if (r == 0) {
        // num is Sequence | RecodeType
        const uint64_t anum = DecodeFixed64(a.data() + a.size() - 8);
        const uint64_t bnum = DecodeFixed64(b.data() + b.size() - 8);
        if (anum > bnum) {
            r = -1;
        } else if (anum < bnum) {
            r = +1;
        }
    }
    return r;
}

void InternalKeyComparator::FindShortestMiddle(std::string* start, const Slice& limit) const {
    Slice start_user = ExtractUserKey(*start);
    Slice limit_user = ExtractUserKey(limit);
    std::string tmp(start_user.data(),start_user.size());
    user_cmp_->FindShortestMiddle(&tmp, limit_user);
    if (tmp.size() < start_user.size() && Compare(start_user, tmp) < 0) {
        PutFixed64(&tmp, PackSequenceAndType(KMaxSequenceNum, KTypeLookup));
        start->swap(tmp);
    }
}

void InternalKeyComparator::FindShortestBigger(std::string* start) const {
    Slice start_user = ExtractUserKey(*start);
    std::string tmp(start->data(), start->size());
    user_cmp_->FindShortestBigger(&tmp);
    if (tmp.size() < start_user.size() && Compare(start_user, tmp) < 0) {
        PutFixed64(&tmp, PackSequenceAndType(KMaxSequenceNum, KTypeLookup));
        start->swap(tmp);
    }
}

}