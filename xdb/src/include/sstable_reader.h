#ifndef STORAGE_XDB_INCLUDE_SSTABLE_READER_H_
#define STORAGE_XDB_INCLUDE_SSTABLE_READER_H_

#include "include/option.h"
#include "util/file.h"
namespace xdb {

class Footer;
class Iterator;

class SSTableReader {
 public:
    ~SSTableReader();

    SSTableReader(const SSTableReader&) = delete;
    SSTableReader operator=(const SSTableReader&) = delete;
    
    static Status Open(const Option& option, RandomReadFile* file,
        uint64_t file_size, SSTableReader** table);

    Iterator* NewIterator(const ReadOption& option) const;
 private:
    struct Rep;
    friend class TableCache;

    explicit SSTableReader(Rep* rep) : rep_(rep) {}

    void ReadFilterIndex(const Footer& footer);

    void ReadFilter(const Slice& handle_contents);

    Status InternalGet(const ReadOption& option, const Slice& key, void* arg,
             void (*handle_result)(void*, const Slice&, const Slice&));
    
    static Iterator* ReadBlockHandle(void* arg, const ReadOption& option, const Slice& handle_contents);
    
    Rep* const rep_;
};

}

#endif // STORAGE_XDB_INCLUDE_SSTABLE_READER_H_