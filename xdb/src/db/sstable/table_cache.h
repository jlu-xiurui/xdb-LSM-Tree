#ifndef STORAGE_XDB_DB_SSTABLE_TABLE_CACHE_H_
#define STORAGE_XDB_DB_SSTABLE_TABLE_CACHE_H_


#include "include/cache.h"
#include "include/env.h"
#include "include/option.h"

namespace xdb {

class TableCache {
 public:
 private:
    const std::string name_;
    const Option* option_;
    Env* env_;
    Cache* cache_;
};

}

#endif // STORAGE_XDB_DB_SSTABLE_TABLE_CACHE_H_