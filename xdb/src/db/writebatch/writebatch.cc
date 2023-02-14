#include "db/writebatch/writebatch_helper.h"

namespace xdb {

void WriteBatchHelper::Append(WriteBatch *dst , const WriteBatch *src) {
    SetCount(dst, GetCount(dst) + GetCount(src));
    dst->rep_.append(src->rep_.data() + KHeaderSize, src->rep_.size() - KHeaderSize);
}
void WriteBatch::Clear() {
    rep_.clear();
    rep_.resize(KHeaderSize);
}

void WriteBatchHelper::SetContent(WriteBatch *b, const Slice& content) {
    assert(content.size() > KHeaderSize);
    b->rep_.assign(content.data(), content.size());
}

Slice WriteBatchHelper::GetContent(WriteBatch *b) {
    return b->rep_;
}
WriteBatch::WriteBatch() { Clear(); }

void WriteBatch::Put(const Slice& key, const Slice& value) {
    WriteBatchHelper::SetCount(this, WriteBatchHelper::GetCount(this) + 1);
    rep_.push_back(static_cast<char>(KTypeInsertion));
    PutLengthPrefixedSlice(&rep_,key);
    PutLengthPrefixedSlice(&rep_,value);
}

void WriteBatch::Delete(const Slice& key) {
    WriteBatchHelper::SetCount(this, WriteBatchHelper::GetCount(this) + 1);
    rep_.push_back(static_cast<char>(KTypeDeletion));
    PutLengthPrefixedSlice(&rep_,key);
}

Status WriteBatch::Iterate(Handle* handle) const {
    Slice input(rep_);
    if (input.size() < KHeaderSize) {
        return Status::Corruption("writebatch is too small ( < KHeaderSize)");
    }
    input.remove_prefix(KHeaderSize);
    Slice key, value;
    int count = 0;

    while(!input.empty()) {
        count++;
        RecordType type = static_cast<RecordType>(input[0]);
        input.remove_prefix(1);
        switch (type)
        {
        case KTypeInsertion:
            if (GetLengthPrefixedSlice(&input, &key) &&
                GetLengthPrefixedSlice(&input, &value)) {
                handle->Put(key, value);
            } else {
                return Status::Corruption("writebatch insert record bad");
            }
            break;
        case KTypeDeletion:
            if (GetLengthPrefixedSlice(&input, &key)) {
                handle->Delete(key);
            } else {
                return Status::Corruption("writebatch delete record bad");
            }
            break;
        }
    }
    if (count != WriteBatchHelper::GetCount(this)) {
        return Status::Corruption("writebatch count err");
    } else {
        return Status::OK();
    }
}

Status WriteBatchHelper::InsertMemTable(const WriteBatch* b, MemTable* mem) {
    MemTableInserter inserter;
    inserter.seq_ = WriteBatchHelper::GetSequenceNum(b);
    inserter.mem_ = mem;
    return b->Iterate(&inserter);
}

}