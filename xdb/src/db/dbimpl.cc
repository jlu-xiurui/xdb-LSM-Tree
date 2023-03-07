#include <algorithm>

#include "db/dbimpl.h"
#include "db/writebatch/writebatch_helper.h"
#include "util/filename.h"
#include "db/log/log_reader.h"
#include "include/sstable_builder.h"
#include "db/sstable/table_cache.h"

namespace xdb {

    const int KNumNonTableCache = 10;

    static size_t TableCacheSize(const Option& option) {
        return option.max_open_file - KNumNonTableCache;
    }

    struct DBImpl::Writer {
        explicit Writer(Mutex* mu)
            : batch(nullptr), done(false), sync(false), cv(mu) {}
        Status status;
        WriteBatch* batch;
        bool done;
        bool sync;
        CondVar cv;
    };

    DBImpl::DBImpl(const Option& option, const std::string& name) 
        : name_(name), 
          internal_comparator_(option.comparator),
          option_(option),
          file_lock_(nullptr),
          env_(option.env),
          mem_(nullptr),
          imm_(nullptr),
          log_(nullptr),
          background_cv_(&mu_),
          background_scheduled_(false),
          closed_(false),
          table_cache_(new TableCache(name, option_, TableCacheSize(option_))),
          vset_(new VersionSet(name, &option_, table_cache_)),
          tmp_batch_(new WriteBatch) {}
          
    DBImpl::~DBImpl() {
        mu_.Lock();
        closed_.store(true, std::memory_order_release);
        if (background_scheduled_) {
            background_cv_.Wait();
        }
        mu_.Unlock();
        if (file_lock_ != nullptr) {
            env_->UnlockFile(file_lock_);
        }
        if(mem_ != nullptr) {
            mem_->Unref();
        }
        if(imm_ != nullptr) {
            imm_->Unref();
        }
        delete vset_;
        delete tmp_batch_;
        delete log_;
        delete logfile_;
        delete table_cache_;
    }

    WriteBatch* DBImpl::MergeBatchGroup(Writer** last_writer) {
        mu_.AssertHeld();
        assert(!writers_.empty());
        Writer* first = writers_.front();
        WriteBatch* ret = first->batch;
        assert(ret != nullptr);

        size_t size = WriteBatchHelper::GetSize(ret);

        // Limit the max size. If the write is small,
        // use the lower limit to speed up the responds.
        size_t max_size = 1 << 20;
        if (size <= (1 << 10)) {
            max_size = size + (1 << 10);
        }

        *last_writer = first;
        std::deque<Writer*>::iterator iter = writers_.begin();
        ++iter;
        for (;iter != writers_.end(); ++iter) {
            Writer* w = *iter;
            if (w->sync && !first->sync) {
                break;
            }
            if (w->batch != nullptr) {
                size += WriteBatchHelper::GetSize(w->batch);
                if (size > max_size) {
                    break;
                }
                if (ret == first->batch) {
                    ret = tmp_batch_;
                    assert(WriteBatchHelper::GetCount(ret) == 0);
                    WriteBatchHelper::Append(ret, first->batch);
                }
                WriteBatchHelper::Append(ret, first->batch);
            }
            *last_writer = w;
        }
        return ret;
    }

    Status DBImpl::Get(const ReadOption& option,const Slice& key, std::string* value) {
        Status status;
        MutexLock l(&mu_);
        SequenceNum seq = vset_->LastSequence();
        Version* current = vset_->Current();

        mem_->Ref();
        if (imm_ != nullptr) imm_->Ref();
        current->Ref();

        {
            mu_.Unlock();
            LookupKey lkey(key, seq);
            if (!mem_->Get(lkey,value,&status)) {
                // not found in mem
            } else if (imm_ != nullptr && !imm_->Get(lkey,value,&status)) {
                // not found in imm
            } else {
                status = current->Get(option, lkey, value);
            }
            mu_.Lock();
        }
        mem_->Unref();
        if (imm_ != nullptr) imm_->Unref();
        current->Unref();
        return status;
    }

    Status DB::Open(const Option& option, const std::string& name, DB** ptr) {
        *ptr = nullptr;

        DBImpl* impl = new DBImpl(option, name);
        impl->mu_.Lock();
        VersionEdit edit;
        Status s = impl->Recover(&edit);
        if (s.ok()) {
            WritableFile* log_file;
            uint64_t log_number = impl->vset_->NextFileNumber();
            s = option.env->NewWritableFile(LogFileName(name,log_number), &log_file);
            if (s.ok()) {
                edit.SetLogNumber(log_number);
                impl->logfile_ = log_file;
                impl->logfile_number_ = log_number;
                impl->log_ = new log::Writer(log_file);
                impl->mem_ = new MemTable(impl->internal_comparator_);
                impl->mem_->Ref();
            }
        }
        if (s.ok()) {
            s = impl->vset_->LogAndApply(&edit, &impl->mu_);
        }
        impl->mu_.Unlock();
        if (s.ok()) {
            *ptr = impl;
        } else {
            delete impl;
        }
        return s;
    }
    
    Status DBImpl::Put(const WriteOption& option,const Slice& key, const Slice& value) {
        WriteBatch batch;
        batch.Put(key, value);
        return Write(option, &batch);
    }

    Status DBImpl::Delete(const WriteOption& option,const Slice& key) {
        WriteBatch batch;
        batch.Delete(key);
        return Write(option, &batch);
    }

    Status DBImpl::Write(const WriteOption& option,WriteBatch* batch) {
        Writer w(&mu_);
        w.batch = batch;
        w.done = false;
        w.sync = option.sync;

        MutexLock l(&mu_);

        writers_.push_back(&w);
        while (!w.done && &w != writers_.front()) {
            w.cv.Wait();
        }
        if (w.done) {
            return w.status;
        }
        
        // merge the writebatch
        Status status = MakeRoomForWrite();
        Writer* last_writer = &w;
        SequenceNum last_seq = vset_->LastSequence();
        if (batch != nullptr) {
            WriteBatch* merged_batch = MergeBatchGroup(&last_writer);
            WriteBatchHelper::SetSequenceNum(merged_batch, last_seq + 1);
            last_seq += WriteBatchHelper::GetCount(merged_batch);
            {
                // only one thread can reach here once time 
                mu_.Unlock();
                status = log_->AddRecord(WriteBatchHelper::GetContent(merged_batch));
                bool sync_error = false;
                if (status.ok() && option.sync) {
                    status = logfile_->Sync();
                    if (!status.ok()) {
                        sync_error = true;
                    }
                }
                if (status.ok()) {
                    status = WriteBatchHelper::InsertMemTable(merged_batch, mem_);
                }
                mu_.Lock();
                if (sync_error) {
                    RecordBackgroundError(status);
                }
            }
            if (merged_batch == tmp_batch_) {
                tmp_batch_->Clear();
            }
            vset_->SetLastSequence(last_seq);
        }
        while (true) {
            Writer* done_writer = writers_.front();
            writers_.pop_front();
            if (done_writer != &w) {
                done_writer->done = true;
                done_writer->status = status;
                done_writer->cv.Signal();
            }
            if (done_writer == last_writer) break;
        }
        if (!writers_.empty()) {
            writers_.front()->cv.Signal();
        }
        //std::cout<<"thread "<< std::this_thread::get_id() <<"done" <<std::endl;
        return status;
    }

    Status DBImpl::MakeRoomForWrite() {
        mu_.AssertHeld();
        Status s;
        while(true) {
            if (!background_status_.ok()) {
                s = background_status_;
                break;
            } else if (mem_->ApproximateSize() <= option_.write_mem_size) {
                // there is enough room for write
                break;
            } else if (imm_ != nullptr) {
                // a memtable is being compact as SStable
                background_cv_.Wait();
            } else {
                uint64_t log_number = vset_->NextFileNumber();
                WritableFile* file;
                s = env_->NewWritableFile(LogFileName(name_, log_number), &file);
                if (!s.ok()) {
                    break;
                }
                delete log_;
                s = logfile_->Close();
                if (!s.ok()) {
                    RecordBackgroundError(s);
                }
                delete logfile_;

                logfile_ = file;
                log_ = new log::Writer(file);
                imm_ = mem_;
                mem_ = new MemTable(internal_comparator_);
                mem_->Ref();
                MayScheduleCompaction();
            }
        }
        return s;
    }

    Status DBImpl::Recover(VersionEdit* edit) {
        mu_.AssertHeld();

        // Check if the DB is locked, to avoid multi-user access.
        env_->CreatDir(name_);
        Status s = env_->LockFile(LockFileName(name_), &file_lock_);
        if (!s.ok()) {
            return s;
        }
        // check if the DB has been opened
        if (!env_->FileExist(CurrentFileName(name_))) {
            // if not opened, initialize the CURRENT.
            s = Initialize();
            if (!s.ok()) {
                return Status::Corruption("DB initialize fail");
            }
        }

        s = vset_->Recover();
        if (!s.ok()) {
            return s;
        }
        uint64_t min_log = vset_->LogNumber();
        uint64_t number;
        FileType type;
        std::vector<uint64_t> log_numbers;
        std::vector<std::string> filenames;
        s = env_->GetChildren(name_, &filenames);
        if (!s.ok()) {
            return s;
        }
        for (auto& filename : filenames) {
            if (ParseFilename(filename,&number,&type)) {
                if (type == KLogFile && (number >= min_log)) {
                    log_numbers.push_back(number);
                }
            }
        }
        std::sort(log_numbers.begin(),log_numbers.end());

        SequenceNum max_sequence(0);
        for (size_t i = 0; i < log_numbers.size(); i++) {
            s = RecoverLogFile(log_numbers[i], &max_sequence, edit);
            if (!s.ok()) {
                return s;
            }
            vset_->MarkFileNumberUsed(log_numbers[i]);
        }
        if (vset_->LastSequence() < max_sequence) {
            vset_->SetLastSequence(max_sequence);
        }
        return Status::OK();
    }

    Status DBImpl::Initialize() {
        VersionEdit edit;
        edit.SetComparatorName(internal_comparator_.UserComparator()->Name());
        edit.SetLastSequence(0);
        edit.SetLogNumber(0);
        edit.SetNextFileNumber(2);

       
        std::string meta_file_name = MetaFileName(name_, 1);
        WritableFile* file = nullptr;
        Status s = env_->NewWritableFile(meta_file_name, &file);
        if (!s.ok()) {
            return s;
        }

        log::Writer writer(file);
        std::string record;
        edit.EncodeTo(&record);
        s = writer.AddRecord(record);
        if (s.ok()) {
            s = file->Sync();
        }
        if (s.ok()) {
            s = file->Close();
        }
        delete file;
        if (s.ok()) {
            s = SetCurrentFile(env_, name_, 1);
        } else {
            env_->RemoveFile(meta_file_name);
        }
        return s;
    }

    Status DBImpl::RecoverLogFile(uint64_t number, SequenceNum* max_sequence
            ,VersionEdit* edit) {
        mu_.AssertHeld();

        std::string filename = LogFileName(name_, number);
        SequentialFile* file;
        Status s = env_->NewSequentialFile(filename, &file);
        if (!s.ok()) {
            return s;
        }
        log::Reader reader(file, true, 0);
        std::string buffer;
        Slice record;
        WriteBatch batch;
        MemTable* mem = nullptr;
        while(reader.ReadRecord(&record, &buffer)) {
            WriteBatchHelper::SetContent(&batch, record);
            if (mem == nullptr){
                mem = new MemTable(internal_comparator_);
                mem->Ref();
            }
            s = WriteBatchHelper::InsertMemTable(&batch, mem);
            if (!s.ok()) {
                break;
            }
            SequenceNum last_seq = WriteBatchHelper::GetSequenceNum(&batch)
                                    + WriteBatchHelper::GetCount(&batch);
            if (last_seq > *max_sequence) {
                *max_sequence = last_seq;
            }

            if (mem->ApproximateSize() > option_.write_mem_size) {
                s = WriteLevel0SSTable(mem, edit);
                mem->Unref();
                mem = nullptr;
                if (!s.ok()) {
                    break;
                }
            }
        }

        if (mem != nullptr) {
            if (s.ok()) {
                s = WriteLevel0SSTable(mem, edit);
            }
        }
        delete file;
        return s;
    }

    Status DestoryDB(const Option& option, const std::string& name) {
        Env* env = option.env;
        std::vector<std::string> filenames;
        Status s = env->GetChildren(name, &filenames);
        if (!s.ok()) {
            return s;
        }
        FileLock* lock;
        const std::string lock_filename = LockFileName(name);
        s = env->LockFile(lock_filename, &lock);
        if (!s.ok()) {
            return s;
        }
        for (auto&& filename : filenames) {
            uint64_t number;
            FileType type;
            if(ParseFilename(filename, &number, &type) 
                    && type != KLockFile) {
                Status del_s = env->RemoveFile(name + "/" + filename);
                if (!del_s.ok() && s.ok() ) {
                    s = del_s;
                }
            }
        }
        env->UnlockFile(lock);
        env->RemoveFile(lock_filename);
        env->RemoveDir(name);
        return s;
    }

    Status DBImpl::WriteLevel0SSTable(MemTable* mem, VersionEdit* edit) {
        mu_.AssertHeld();
        FileMeta meta;
        meta.number = vset_->NextFileNumber();
        files_writing_.insert(meta.number);
        Iterator* iter = mem->NewIterator();

        Status s;
        {
            mu_.Unlock();
            s = BuildSSTable(name_, option_, iter, &meta);
            mu_.Lock();
        }
        delete iter;
        files_writing_.erase(meta.number);

        if (s.ok() && meta.file_size > 0) {
            edit->AddFile(0, meta.number, meta.file_size,
                    meta.smallest, meta.largest);
        }
        return s;
    }
    void DBImpl::MayScheduleCompaction() {
        mu_.AssertHeld();
        if (closed_.load(std::memory_order_acquire)) {
            // DB is being deleted
        } else if (background_scheduled_) {
            // only one compaction could running
        } else if (!background_status_.ok()) {
            // compaction cause a error
        } else if (imm_ == nullptr 
            /*&& version need compaction*/) {
            // noting to do
        } else {
            background_scheduled_ = true;
            env_->Schedule(&DBImpl::CompactionSchedule, this);
        }

    }

    void DBImpl::CompactionSchedule(void* db) {
        reinterpret_cast<DBImpl*>(db)->BackgroundCompactionCall();
    }

    void DBImpl::BackgroundCompactionCall() {
        MutexLock l(&mu_);
        if (closed_.load(std::memory_order_acquire)) {
            // DB is being deleted
        } else if (!background_status_.ok()) {
            // compaction cause a error
        } else {
            BackgroundCompaction();
        }
        background_scheduled_ = false;
        background_cv_.SignalAll();
    }

    void DBImpl::BackgroundCompaction() {
        mu_.AssertHeld();
        if (imm_ != nullptr) {
            CompactionMemtable();
        }
    }

    void DBImpl::CompactionMemtable() {
        mu_.AssertHeld();
        VersionEdit edit;
        Status s = WriteLevel0SSTable(imm_, &edit);

        if (s.ok() && closed_.load(std::memory_order_acquire)) {
            s = Status::Corruption("DB is closed during compaction memtable");
        }
        if (s.ok()) {
            // early log is unuseful
            edit.SetLogNumber(logfile_number_);
            s = vset_->LogAndApply(&edit, &mu_);
        }
        if (s.ok()) {
            imm_->Unref();
            imm_ = nullptr;
            GarbageFilesClean();
        } else {
            RecordBackgroundError(s);
        }
    } 

    void DBImpl::RecordBackgroundError(Status s) {
        mu_.AssertHeld();
        if (background_status_.ok()) {
            background_status_ = s;
            background_cv_.SignalAll();
        }
    }

    void DBImpl::GarbageFilesClean() {
        mu_.AssertHeld();
        if (!background_status_.ok()) {
            return;
        }
        std::set<uint64_t> live;
        vset_->AddLiveFiles(&live);

        std::vector<std::string> filenames;
        env_->GetChildren(name_, &filenames);
        uint64_t number;
        FileType type;
        std::vector<std::string> file_delete;

        for (const std::string& filename : filenames) {
            if (ParseFilename(filename, &number, &type)) {
                bool keep = true;
                switch(type) {
                    case KLogFile:
                        keep = number >= vset_->LogNumber();
                        break;
                    case KMetaFile:
                        keep = number >= vset_->MetaFileNumber();
                        break;
                    case KSSTableFile:
                        keep = live.find(number) != live.end();
                        break;
                    case KTmpFile:
                        keep = false;
                        break;
                    case KCurrentFile:
                    case KLockFile:
                        keep = true;
                        break;
                }
                if (!keep) {
                    file_delete.push_back(filename);
                }
            }
        }
        mu_.Unlock();
        for (const std::string& filename : file_delete) {
            env_->RemoveFile(name_ + "/" + filename);
        }
        mu_.Lock();
    }
}