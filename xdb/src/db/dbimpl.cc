#include <algorithm>

#include "db/dbimpl.h"
#include "db/writebatch/writebatch_helper.h"
#include "util/filename.h"
#include "db/log/log_reader.h"

namespace xdb {
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
          log_(nullptr),
          vset_(new VersionSet(name, &option_)),
          tmp_batch_(new WriteBatch) {}
    DBImpl::~DBImpl() {
        if (file_lock_ != nullptr) {
            env_->UnlockFile(file_lock_);
        }
        if(mem_ != nullptr) {
            mem_->Unref();
        }
        delete vset_;
        delete tmp_batch_;
        delete log_;
        delete logfile_;
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

    Status DBImpl::Get(const Slice& key, std::string* value) {
        Status status;
        MutexLock l(&mu_);
        SequenceNum seq = vset_->LastSequence();
        mem_->Ref();
        {
            mu_.Unlock();
            LookupKey lkey(key, seq);
            if (!mem_->Get(lkey,value,&status)) {
                status = Status::NotFound(Slice());
            }
            mu_.Lock();
        }
        mem_->Unref();
        return status;
    }

    Status DB::Open(const Option& option, const std::string& name, DB** ptr) {
        *ptr = nullptr;

        DBImpl* impl = new DBImpl(option, name);
        impl->mu_.Lock();
        Status s = impl->Recover();
        if (s.ok() && impl->mem_ == nullptr) {
            WritableFile* log_file;
            s = option.env->NewWritableFile(LogFileName(name,0), &log_file);
            if (s.ok()) {
                impl->logfile_ = log_file;
                impl->log_ = new log::Writer(log_file);
                impl->mem_ = new MemTable(impl->internal_comparator_);
                impl->mem_->Ref();
            }
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
        //std::cout<<"thread "<< std::this_thread::get_id() <<"start" <<std::endl;
        writers_.push_back(&w);
        while (!w.done && &w != writers_.front()) {
            w.cv.Wait();
        }
        if (w.done) {
            return w.status;
        }
        
        // merge the writebatch
        Status status;
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
                if (status.ok() && option.sync) {
                    status = logfile_->Sync();
                }
                if (status.ok()) {
                    status = WriteBatchHelper::InsertMemTable(merged_batch, mem_);
                }
                mu_.Lock();
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

    Status DBImpl::Recover() {
        mu_.AssertHeld();

        // Check if the DB is locked, to avoid multi-user access.
        env_->CreatDir(name_);
        Status s = env_->LockFile(LockFileName(name_), &file_lock_);
        if (!s.ok()) {
            return s;
        }
        std::vector<std::string> filenames;
        s = env_->GetChildren(name_, &filenames);
        if (!s.ok()) {
            return s;
        }

        // check if the DB has been opened
        if (!env_->FileExist(CurrentFileName(name_))) {
            
        }

        s = vset_->Recover();
        uint64_t number;
        FileType type;
        std::vector<uint64_t> log_numbers;
        for (auto& filename : filenames) {
            if (ParseFilename(filename,&number,&type)) {
                if (type == KLogFile) {
                    log_numbers.push_back(number);
                }
            }
        }
        std::sort(log_numbers.begin(),log_numbers.end());
        SequenceNum max_sequence(0);
        for (size_t i = 0; i < log_numbers.size(); i++) {
            s = RecoverLogFile(log_numbers[i], (i == log_numbers.size() - 1), &max_sequence);
            if (!s.ok()) {
                return s;
            }
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
    }

    Status DBImpl::RecoverLogFile(uint64_t number, bool last_log, SequenceNum* max_sequence) {
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
        }
        if (s.ok() && last_log) {
            assert(mem_ == nullptr);
            assert(log_ == nullptr);
            uint64_t logfile_size;
            if (env_->FileSize(filename, &logfile_size).ok() &&
                env_->NewAppendableFile(filename, &logfile_).ok()) {
                log_ = new log::Writer(logfile_, logfile_size);
                if (mem != nullptr) {
                    mem_ = mem;
                    mem = nullptr;
                } else {
                    mem_ = new MemTable(internal_comparator_);
                    mem_->Ref();
                }
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
}