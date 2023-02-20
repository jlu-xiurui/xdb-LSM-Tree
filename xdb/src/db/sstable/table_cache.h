#ifndef STORAGE_XDB_DB_SSTABLE_TABLE_CACHE_H_
#define STORAGE_XDB_DB_SSTABLE_TABLE_CACHE_H_


#include "include/cache.h"

namespace xdb {

class TableCache {
 public:
 private:
    Cache* cache_;
};

}

#endif // STORAGE_XDB_DB_SSTABLE_TABLE_CACHE_H_