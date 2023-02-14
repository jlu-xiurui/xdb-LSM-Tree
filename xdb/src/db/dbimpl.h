#ifndef STORAGE_XDB_DB_DBIMPL_H_
#define STORAGE_XDB_DB_DBIMPL_H_

#include <deque>

#include "include/db.h"
#include "include/env.h"
#include "db/memtable/memtable.h"
#include "db/log/log_writer.h"

namespace xdb {

class DBImpl : public DB {
 public:
    DBImpl(const Option& option, const std::string& name);

    ~DBImpl() override;

    Status Get(const Slice& key, std::string* value) override;

    Status Put(const WriteOption& option,const Slice& key, const Slice& value) override;

    Status Delete(const WriteOption& option,const Slice& key) override;

    Status Write(const WriteOption& option,WriteBatch* batch) override;

    
 private:
    friend class DB;
    struct Writer;

    WriteBatch* MergeBatchGroup(Writer** last_writer);
    
    Status Recover();
    
    Status RecoverLogFile(uint64_t number, bool last_log, SequenceNum* max_sequence);

    const std::string name_;
    const InternalKeyComparator internal_comparator_;
    FileLock* file_lock_;
    Env* env_;
    MemTable* mem_;
    log::Writer* log_;
    WritableFile* logfile_;
    SequenceNum last_seq_;
    Mutex mu_;
    
    WriteBatch* tmp_batch_ GUARDED_BY(mu_);
    std::deque<Writer*> writers_ GUARDED_BY(mu_);
};

}

#endif // STORAGE_XDB_DB_DBIMPL_H_