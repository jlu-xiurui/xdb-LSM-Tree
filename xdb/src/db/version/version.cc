#include <cassert>

#include <algorithm>
#include <cassert>

#include "db/version/version.h"
#include "db/version/version_edit.h"
#include "include/env.h"
#include "util/filename.h"
#include "db/log/log_reader.h"

namespace xdb {
    VersionSet::VersionSet(const std::string name, const Option* option, TableCache* cache,
            const InternalKeyComparator* cmp)
        : name_(name),
          option_(option),
          env_(option->env),
          icmp_(*cmp),
          table_cache_(cache),
          dummy_head_(this),
          current_(nullptr),
          log_number_(0),
          last_sequence_(0),
          next_file_number_(2),
          meta_file_number_(0),
          meta_log_file_(nullptr),
          meta_log_writer_(nullptr) {
        AppendVersion(new Version(this));
    }
    VersionSet::~VersionSet() {
        current_->Unref();
        assert(dummy_head_.next_ == &dummy_head_);
        delete meta_log_file_;
        delete meta_log_writer_;
    }
    Version::~Version() {
        assert(refs_ == 0);
        next_->prev_ = prev_;
        prev_->next_ = next_;

        for (int level = 0; level < config::KNumLevels; level++) {
            for (FileMeta* meta : files_[level]) {
                assert(meta->refs > 0);
                meta->refs--;
                if (meta->refs == 0) {
                    delete meta;
                }
            }
        }
    }

    enum SaverState {
        KNotFound,
        KFound,
        KDeleted,
        KCorrupt,
    };

    struct GetSaver {
        Slice user_key;
        std::string* result;
        const Comparator* user_cmp;
        SaverState state;
    };

    static void SaveResult(void* arg, const Slice& key, const Slice& value) {
        GetSaver* saver = reinterpret_cast<GetSaver*>(arg);
        ParsedInternalKey parsed_key;
        if (!ParseInternalKey(key, &parsed_key)) {
            saver->state = KCorrupt;
        } else {
            if (saver->user_cmp->Compare(parsed_key.user_key_, saver->user_key) == 0) {
                saver->state = (parsed_key.type_ == KTypeInsertion ? KFound : KDeleted);
                if (saver->state == KFound) {
                    saver->result->assign(value.data(), value.size());
                }
            }
        }
    }

    static bool NewFirst(FileMeta* a, FileMeta* b) {
        return a->number > b->number;
    }

    // find first file that "largest >= internal_key"
    static size_t FindFile(std::vector<FileMeta*> files, const Slice& user_key, 
            const Comparator* ucmp) {
        size_t lo = 0;
        size_t hi = files.size();
        while (lo < hi) {
            // if (lo == hi - 1), mid will be lo.
            // to avoid dead loop
            size_t mid = (lo + hi) / 2;
            FileMeta* meta = files[mid];
            if (ucmp->Compare(user_key, meta->largest.user_key()) > 0) {
                // all file before or at mid is "largest < internal_key"
                lo = mid + 1;
            } else {
                // mid is "largest >= internal_key",
                // all file after it is unuseful
                hi = mid;
            }
        }
        return hi;
    }

    void Version::ForEachFile(Slice user_key, Slice internal_key, void* arg,
            bool(*fun)(void*, int, FileMeta*)) {
        const Comparator* ucmp = vset_->icmp_.UserComparator();
        std::vector<FileMeta*> tmp;
        for (FileMeta* meta : files_[0]) {
            if (ucmp->Compare(user_key, meta->largest.user_key()) <= 0 &&
                    ucmp->Compare(user_key, meta->smallest.user_key()) >= 0) {
                tmp.push_back(meta);
            }
        }
        if (!tmp.empty()) {
            std::sort(tmp.begin(), tmp.end(), NewFirst);
            for (FileMeta* meta : tmp) {
                if (!(*fun)(arg, 0, meta)) {
                    return;
                }
            }
        }
        for (int level = 1; level < config::KNumLevels; level++) {
            if (files_[level].size() == 0) {
                continue;
            }
            size_t index = FindFile(files_[level], user_key, ucmp);
            if (index < files_[level].size()) {
                FileMeta* meta = files_[level][index];
                if (ucmp->Compare(user_key, meta->smallest.user_key()) >= 0) {
                    if (!(*fun)(arg, level, meta)) {
                        return;
                    }
                }
            }
        }

    }
    Status Version::Get(const ReadOption& option, const LookupKey& key, std::string* result) {
        struct State {
            GetSaver saver;
            const ReadOption* option;
            Slice internal_key;
            Status s;
            VersionSet* vset;
            bool found;

            static bool SSTableMatch(void* arg, int level, FileMeta* meta) {
                State* state = reinterpret_cast<State*>(arg);
                state->s = state->vset->table_cache_->Get(*state->option, meta->number,
                        meta->file_size, state->internal_key, &state->saver, &SaveResult);
                if (!state->s.ok()) {
                    state->found = true;
                    return false;
                }
                switch (state->saver.state) {
                    case KCorrupt:
                        state->found = true;
                        state->s = Status::Corruption("incorrect parse key for ", state->saver.user_key);
                        return false;
                    case KFound:
                        state->found = true;
                        return false;
                    case KDeleted:
                        return false;
                    case KNotFound:
                        return true; // keep searching
                }
                return false;
            }
        };
        State state;
        state.saver.user_key = key.UserKey();
        state.saver.state = KNotFound;
        state.saver.result = result;
        state.saver.user_cmp = vset_->icmp_.UserComparator();
        
        state.option = &option;
        state.internal_key = key.InternalKey();
        state.vset = vset_;
        state.found = false;

        ForEachFile(state.saver.user_key, state.internal_key, &state,
                &State::SSTableMatch);
        return state.found ? state.s : Status::NotFound("key is not found in sstable");      
    }



    void Version::Ref() { ++refs_;}

    void Version::Unref() {
        assert(refs_ >= 1);
        --refs_;
        if (refs_ == 0) {
            delete this;
        }
    }

    class VersionSet::Builder {
     public:
        Builder(VersionSet* vset, Version* base) : vset_(vset), base_(base) {
            base->Ref();
            BySmallestKey cmp;
            cmp.icmp_ = &vset->icmp_;
            for (int i = 0; i < config::KNumLevels; i++) {
                level_[i].add_files_ = new AddSet(cmp);
            }
        }
        ~Builder() {
            for (int i = 0; i < config::KNumLevels; i++) {
                AddSet* add_files = level_[i].add_files_;
                std::vector<FileMeta*> to_unref;
                to_unref.reserve(add_files->size());
                for (AddSet::iterator it = add_files->begin();
                        it != add_files->end(); ++it) {
                    to_unref.push_back(*it);
                }
                delete add_files;
                for (FileMeta* meta : to_unref) {
                    meta->refs--;
                    if (meta->refs <= 0) {
                        delete meta;
                    }
                }
            }
            base_->Unref();
        }
        void Apply(const VersionEdit* edit) {
            for (const auto& file : edit->delete_files_) {
                const int level = file.first;
                const uint64_t file_number = file.second;
                level_[level].deleted_files.insert(file_number);
            }
            for (const auto& file : edit->new_files_) {
                const int level = file.first;
                FileMeta* meta = new FileMeta(file.second);
                meta->refs = 1; {
    }
                level_[level].deleted_files.erase(meta->number);
                level_[level].add_files_->insert(meta);
            }
        }
        void SaveTo(Version* v) {
            BySmallestKey cmp;
            cmp.icmp_ = &vset_->icmp_;
            for (int i = 0; i < config::KNumLevels; i++) {
                const std::vector<FileMeta*>& base_files = base_->files_[i];
                std::vector<FileMeta*>::const_iterator base_iter = base_files.begin();
                std::vector<FileMeta*>::const_iterator base_end = base_files.end();
                const AddSet* add_files = level_[i].add_files_;
                v->files_[i].reserve(base_files.size() + add_files->size());
                for (const auto& add_file : *add_files) {
                    for (std::vector<FileMeta*>::const_iterator base_pos = 
                                std::upper_bound(base_iter, base_end, add_file, cmp);
                            base_iter != base_pos; ++base_iter) {
                        AddFile(v, i, *base_iter);
                    }
                    AddFile(v, i, add_file);
                }
                for (;base_iter != base_end; ++base_iter) {
                    AddFile(v, i, *base_iter);
                }
            }
        }
        void AddFile(Version* v, int level, FileMeta* meta) {
            if (level_[level].deleted_files.count(meta->number)) {
                return;
            }
            std::vector<FileMeta*>* files = &v->files_[level];
            if (level > 0 && files->empty()) {
                assert(vset_->icmp_.Compare((*files)[files->size() - 1]->largest,
                        meta->smallest) < 0);
            }
            meta->refs++;
            files->push_back(meta);
        }
     private:
        struct BySmallestKey {
            const InternalKeyComparator* icmp_;

            bool operator()(FileMeta* f1, FileMeta* f2) const {
                int r = icmp_->Compare(f1->smallest, f2->smallest);
                if (r != 0) {
                    return (r < 0);
                } else {
                    return (f1->number < f2->number);
                }
            }
        };
        using AddSet = std::set<FileMeta*, BySmallestKey>;
        struct LevelState {
            std::set<uint64_t> deleted_files;
            AddSet* add_files_;
        };

        Version* base_;
        VersionSet* vset_;
        LevelState level_[config::KNumLevels];
    };

    Status VersionSet::Recover() {
        std::string current;
        Status s = ReadStringFromFile(env_, &current, CurrentFileName(name_));
        if (!s.ok()) {
            return s;
        }
        SequentialFile* file;
        s = env_->NewSequentialFile(current, &file);
        if (!s.ok()) {
            if (s.IsNotFound()) {
                return Status::Corruption("CURRENT file points to a not-existing Meta file");
            }
            return s;
        }
        uint64_t log_number = 0;
        SequenceNum last_sequence = 0;
        uint64_t next_file_number = 0;
        std::string comparator_name;
        bool has_log_number = false;
        bool has_last_sequence = false;
        bool has_next_file_number = false;
        bool has_comparator_name = false;
        Builder builder(this, current_);

        {
            log::Reader reader(file, true, 0);
            Slice slice;
            std::string buffer;
            while(reader.ReadRecord(&slice, &buffer) && s.ok()) {
                VersionEdit edit;
                s = edit.DecodeFrom(slice);
                if (s.ok()) {
                    if (edit.has_comparator_name_ &&
                            edit.comparator_name_ != icmp_.UserComparator()->Name()) {
                        s = Status::Corruption(edit.comparator_name_ + "don't match " +
                                icmp_.UserComparator()->Name());
                    }
                }
                if (s.ok()) {
                    builder.Apply(&edit);
                }
                if (edit.has_log_number_) {
                    has_log_number = true;
                    log_number = edit.log_number_;
                }
                if (edit.has_last_sequence_) {
                    has_last_sequence = true;
                    last_sequence = edit.last_sequence_;
                }
                if (edit.has_next_file_number_) {
                    has_next_file_number = true;
                    next_file_number = edit.next_file_number_;
                }
            }
        }
        delete file;

        if (s.ok()) {
            if (!has_log_number) {
                s = Status::Corruption("no log_number in meta file");
            }
            if (!has_last_sequence) {
                s = Status::Corruption("no last_sequence in meta file");
            }
            if (!has_next_file_number) {
                s = Status::Corruption("no next_file_number in meta file");
            }
            MarkFileNumberUsed(log_number);
        }

        if (s.ok()) {
            Version* v = new Version(this);
            builder.SaveTo(v);
            AppendVersion(v);
            log_number_ = log_number;
            meta_file_number_ = next_file_number;
            last_sequence_ = last_sequence;
            next_file_number_ = next_file_number + 1;
        }
        return s;
    }

    Status VersionSet::LogAndApply(VersionEdit* edit, Mutex* mu) {
        if (edit->has_log_number_) {
            assert(edit->log_number_ >= log_number_);
            assert(edit->log_number_ < next_file_number_);
        } else {
            edit->SetLogNumber(log_number_);
        }
        edit->SetLastSequence(last_sequence_);
        edit->SetNextFileNumber(next_file_number_);

        Version* v = new Version(this);

        Builder builder(this, current_);
        builder.Apply(edit);
        builder.SaveTo(v);

        Status s;
        std::string meta_file_name;

        bool initialize = false;
        if (meta_log_writer_ == nullptr) {
            assert(meta_log_file_ == nullptr);
            initialize = true;
            meta_file_name = MetaFileName(name_, meta_file_number_);
            s = env_->NewWritableFile(meta_file_name, &meta_log_file_);
            if (s.ok()) {
                meta_log_writer_ = new log::Writer(meta_log_file_);
                s = WriteSnapShot(meta_log_writer_);
            }
        }

        {
            // NOTE: Only one Compaction is running at some time
            // so unlock is safe here;
            mu->Unlock();
            if (s.ok()) {
                std::string record;
                edit->EncodeTo(&record);
                s = meta_log_writer_->AddRecord(record);
                if (s.ok()) {
                    s = meta_log_file_->Sync();
                }
            }
            if (s.ok() && initialize) {
                s = SetCurrentFile(env_, name_, meta_file_number_);
            }
            mu->Lock();
        }

        if (s.ok()) {
            AppendVersion(v);
            log_number_ = edit->log_number_;
        } else {
            delete v;
            if (initialize) {
                delete meta_log_file_;
                delete meta_log_writer_;
                env_->RemoveFile(meta_file_name);
            }
        }
        return s;
    }

    Status VersionSet::WriteSnapShot(log::Writer* writer) {
        VersionEdit edit;

        edit.SetComparatorName(icmp_.UserComparator()->Name());

        for (int level = 0; level < config::KNumLevels; level++) {
            const std::vector<FileMeta*>& files = current_->files_[level];
            for (const FileMeta* meta : files) {
                edit.AddFile(level, meta->number, meta->file_size,
                        meta->smallest, meta->largest);
            }
        }

        std::string record;
        edit.EncodeTo(&record);
        return writer->AddRecord(record);
    }

    void VersionSet::AppendVersion(Version* v) {
        assert(v->refs_ == 0);
        assert(current_ != v);

        if (current_ != nullptr) {
            current_->Unref();
        }
        v->Ref();
        current_ = v;

        v->next_ = &dummy_head_;
        v->prev_ = dummy_head_.prev_;
        v->next_->prev_ = v;
        v->prev_->next_ = v;
    }

    void VersionSet::AddLiveFiles(std::set<uint64_t>* live) {
        for (Version* v = dummy_head_.next_;v != &dummy_head_; v = v->next_ ) {
            for (int level = 0; level < config::KNumLevels; level++) {
                const std::vector<FileMeta*>& files_ = v->files_[level];
                for (const FileMeta* file : files_) {
                    live->insert(file->number);
                }
            }
        }
    }

    
}