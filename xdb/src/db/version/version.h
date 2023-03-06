#ifndef STORAGE_XDB_DB_VERSION_VERSION_H_
#define STORAGE_XDB_DB_VERSION_VERSION_H_

#include <cassert>

#include "db/version/version_edit.h"
#include "include/iterator.h"
#include "include/comparator.h"
#include "db/format/dbformat.h"
#include "db/log/log_writer.h"
#include "util/mutex.h"
#include "include/option.h"
#include "db/sstable/table_cache.h"

namespace xdb {

class VersionSet;

class Version {
 public:
    void Ref();
    void Unref();

    Status Get(const ReadOption& option, const LookupKey& key, std::string* result);
 private:
    friend class VersionSet;

    explicit Version(VersionSet* vset) : vset_(vset), next_(this),
            prev_(this), refs_(0) {}
    
    Version(const Version&) = delete;
    Version& operator=(const Version&) = delete;

    ~Version();
    
    
    VersionSet* vset_;
    Version* next_;
    Version* prev_;
    int refs_;

    std::vector<FileMeta*> files_[config::KNumLevels];
};

class VersionSet {
 public:
    VersionSet(const std::string name, const Option* option, 
         TableCache* cache);
    
    VersionSet(const VersionSet&) = delete;
    VersionSet& operator=(const VersionSet&) = delete;

    ~VersionSet();

    Status LogAndApply(VersionEdit* edit, Mutex* mu) 
        EXCLUSIVE_LOCKS_REQUIRED(mu);
    
    Status Recover();
    
    uint64_t NextFileNumber() { return next_file_number_++; }

    uint64_t LastSequence() const { return last_sequence_; }

    uint64_t LogNumber() const { return log_number_; }
    
    uint64_t MetaFileNumber() const { return meta_file_number_; }
    
    void MarkFileNumberUsed(uint64_t number) {
      if (number >= next_file_number_) {
         next_file_number_ = number + 1;
      }
    }

    void SetLastSequence(uint64_t s) {
      assert(s >= last_sequence_);
      last_sequence_ = s;
    }

    void AddLiveFiles(std::set<uint64_t>* live);
 private:
    class Builder;
    
    friend class Version;

    Status WriteSnapShot(log::Writer* writer);
    
    void AppendVersion(Version* v);

    const std::string name_;
    const Option* option_;
    Env* env_;
    const InternalKeyComparator icmp_;

    TableCache* table_cache_;
    Version dummy_head_;
    Version* current_;

    uint64_t log_number_;
    SequenceNum last_sequence_;
    uint64_t next_file_number_;
    uint64_t meta_file_number_;
    
    WritableFile* meta_log_file_;
    log::Writer* meta_log_writer_;
};

}

#endif // STORAGE_XDB_DB_VERSION_VERSION_H_