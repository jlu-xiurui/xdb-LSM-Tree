#include "db/sstable/table_cache.h"
#include "include/sstable_reader.h"
#include "util/coding.h"
#include "include/cache.h"
#include "util/filename.h"
#include "include/env.h"

namespace xdb {

struct TableAndFile {
    SSTableReader* table;
    RandomReadFile* file;
};
static void DeleteEntry(const xdb::Slice &key, void *value) {
    TableAndFile* tf = reinterpret_cast<TableAndFile*>(value);
    delete tf->table;
    delete tf->file;
    delete tf;
}

Status TableCache::Get(const ReadOption& option, uint64_t file_number, uint64_t file_size,
            const Slice& key, void* arg, void (*handle_result)(void*, const Slice&, const Slice&)) {
    Cache::Handle* handle = nullptr;
    Status s = FindTable(file_number, file_size, &handle);
    if (s.ok()) {
        SSTableReader* table = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
        s = table->InternalGet(option, key, arg, handle_result);
        cache_->Release(handle);
    }
    return s;
}

void TableCache::Evict(uint64_t file_number) {
    char buf[sizeof(file_number)];
    EncodeFixed64(buf, file_number);
    cache_->Erase(Slice(buf, sizeof(buf)));
}

Status TableCache::FindTable(uint64_t file_number, uint64_t file_size, Cache::Handle** handle) {
    Status s;
    char buf[sizeof(file_number)];
    EncodeFixed64(buf, file_number);
    Slice key(buf, sizeof(buf));
    *handle = cache_->Lookup(key);
    if (*handle == nullptr) {
        std::string filename = SSTableFileName(name_, file_number);
        RandomReadFile* file = nullptr;
        s = env_->NewRamdomReadFile(filename, &file);
        SSTableReader* table = nullptr;
        if (s.ok()) {
            s = SSTableReader::Open(option_, file, file_size, &table);
        }
        if (!s.ok()) {
            delete file;
        } else {
            TableAndFile* tf = new TableAndFile;
            tf->table = table;
            tf->file = file;
            *handle = cache_->Insert(key, tf, 1, &DeleteEntry);
        }
    }
    return s;
}

}
