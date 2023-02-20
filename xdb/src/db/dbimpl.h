#ifndef STORAGE_XDB_DB_DBIMPL_H_
#define STORAGE_XDB_DB_DBIMPL_H_

#include <deque>

#include "include/db.h"
#include "include/env.h"
#include "db/memtable/memtable.h"
#include "db/log/log_writer.h"
#include "db/version/version.h"

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
    
    Status Recover() EXCLUSIVE_LOCKS_REQUIRED(mu_);

    Status Initialize();
    
    Status RecoverLogFile(uint64_t number, bool last_log, SequenceNum* max_sequence);

    const std::string name_;
    const InternalKeyComparator internal_comparator_;
    const Option option_;

    FileLock* file_lock_;
    Env* env_;
    // in memory cache and its write-ahead logger
    MemTable* mem_;
    log::Writer* log_;
    WritableFile* logfile_;
    SequenceNum last_seq_;
    Mutex mu_;
    
    VersionSet* vset_;
    WriteBatch* tmp_batch_ GUARDED_BY(mu_);
    std::deque<Writer*> writers_ GUARDED_BY(mu_);
};

}

#endif // STORAGE_XDB_DB_DBIMPL_H_