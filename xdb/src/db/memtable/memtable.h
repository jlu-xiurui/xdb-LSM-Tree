#ifndef STORAGE_XDB_DB_MEMTABLE_MEMTABLE_H_
#define STORAGE_XDB_DB_MEMTABLE_MEMTABLE_H_

#include "db/format/internal_key.h"
#include "db/memtable/membuffer.h"
#include "db/memtable/skiplist.h"
#include "include/status.h"
#include "include/iterator.h"

namespace xdb {

class MemTable {
 public:
    explicit MemTable(const InternalKeyComparator& cmp);

    MemTable(const MemTable&) = delete;
    MemTable& operator=(const MemTable&) = delete;

    void Ref() { refs_++; };

    void Unref() {
        refs_--;
        if(refs_ <= 0) {
            delete this;
        }
    }
    void Put(SequenceNum seq, RecordType type,
        const Slice& key, const Slice& value);
    
    bool Get(const LookupKey& key, std::string* result, Status* status);

    Iterator* NewIterator();

    size_t ApproximateSize() { return mem_buffer_.MemoryUsed(); }
 private:
    struct KeyComparator {
        const InternalKeyComparator comparator;
        explicit KeyComparator(const InternalKeyComparator& cmp) : comparator(cmp) {}
        int operator()(const char* a, const char* b) const;
    };

    typedef SkipList<const char*, KeyComparator> Table;

    friend class MemTableIterator;

    ~MemTable() { assert(refs_ == 0); };

    KeyComparator comparator_;
    MemBuffer mem_buffer_;
    Table table_;
    int refs_;
};

}

#endif // STORAGE_XDB_DB_MEMTABLE_MEMTABLE_H_