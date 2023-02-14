#ifndef STORAGE_XDB_INCLUDE_WRITEBATCH_H_
#define STORAGE_XDB_INCLUDE_WRITEBATCH_H_


#include "util/coding.h"
#include "include/status.h"
namespace xdb {

class WriteBatch {
 public:
    class Handle {
      public:
         virtual ~Handle() = default;
         virtual void Put(const Slice& key, const Slice& value) = 0;
         virtual void Delete(const Slice& key) = 0;
    };
    WriteBatch();

    ~WriteBatch() = default;

    WriteBatch(const WriteBatch&) = default;

    WriteBatch& operator=(const WriteBatch&) = default;

    void Put(const Slice& key, const Slice& value);

    void Delete(const Slice& key);

    void Clear();

    Status Iterate(Handle* handle) const;
 private:
   friend class WriteBatchHelper;

   std::string rep_;

};


}


#endif // STORAGE_XDB_INCLUDE_WRITEBATCH_H_