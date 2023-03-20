
#include "db/version/merge.h"

namespace xdb {

class MergedIterator : public Iterator {
 public:
    MergedIterator(Iterator** list, size_t num, const Comparator* cmp) :
            list_(list), num_(num), current_(nullptr), cmp_(cmp) {}

    ~MergedIterator() {
        for (int i = 0; i < num_; i++) {
            delete list_[i];
        }
        delete[] list_;
    }
    bool Valid() const override { return current_ != nullptr; }

    Slice Key() const override {
        assert(Valid());
        return (*current_)->Key();
    }

    Slice Value() const override {
        assert(Valid());
        return (*current_)->Value();
    }

    void Next() override {
        assert(Valid());
        (*current_)->Next();
        FindSmallest();
    }

    void Prev() override { assert(false); }

    void Seek(const Slice& key) override {
        for (int i = 0; i < num_; i++) {
            list_[i]->Seek(key);
        }
        FindSmallest();
    }

    void SeekToFirst() override {
        for (int i = 0; i < num_; i++) {
            list_[i]->SeekToFirst();
        }
        FindSmallest();
    }
    void SeekToLast() override { assert(false); }

    Status status() override {
        Status status;
        for (int i = 0; i < num_; i++) {
            status = list_[i]->status();
            if (!status.ok()) {
                return status;
            }
        }
        return Status::OK();
    }
 private:
    void FindSmallest();
    Iterator** list_;
    size_t num_;
    Iterator** current_;
    const Comparator* cmp_;
};

void MergedIterator::FindSmallest() {
    Iterator** smallest = nullptr;
    for (size_t i = 0; i < num_; i++) {
        Iterator** ptr = &list_[i];
        if ((*ptr)->Valid()) {
            if (smallest == nullptr || cmp_->Compare((*ptr)->Key(), (*smallest)->Key()) < 0) {
                smallest = ptr;
            }
        }
    }
    current_ = smallest;
}

Iterator* NewMergedIterator(Iterator** list, size_t num, const Comparator* cmp) {
    return new MergedIterator(list, num, cmp);
}

}