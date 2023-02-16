#include <cassert>

#include <algorithm>
#include <cassert>

#include "db/version/version.h"
#include "db/version/version_edit.h"
#include "include/env.h"
#include "util/filename.h"

namespace xdb {
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
                meta->refs = 1;
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
        bool initialize = false;
        if (meta_log_writer_ == nullptr) {
            assert(meta_log_file_ == nullptr);
            initialize = true;
            std::string meta_file_name = MetaFileName(name_, meta_file_number_);
            s = env_->NewWritableFile(meta_file_name, &meta_log_file_);
            if (s.ok()) {
                meta_log_writer_ = new log::Writer(meta_log_file_);
                s = WriteSnapShot(meta_log_writer_);
            }
        }
        
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
}