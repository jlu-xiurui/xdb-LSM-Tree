#ifndef STORAGE_XDB_DB_MEMTABLE_SKIPLIST_H_
#define STORAGE_XDB_DB_MEMTABLE_SKIPLIST_H_

#include <atomic>
#include <random>
#include <cassert>
#include <iostream>

#include "db/memtable/membuffer.h"
namespace xdb {

template<typename Key, class Comparator>
class SkipList {
 private:
    struct Node;
 public:
    explicit SkipList(Comparator cmp_, MemBuffer* mem_buffer);

    SkipList(const SkipList&) = delete;
    SkipList& operator=(const SkipList&) = delete;

    void Insert(const Key& key);

    bool Contain(const Key& key);

    class Iterator {
     public:
        explicit Iterator(const SkipList* list)
            : list_(list), node_(nullptr) {}

        bool Valid() const { return node_ != nullptr; }

        const Key& key() const {
            assert(Valid());
            return node_->key_; 
        }

        void Next() {
            assert(Valid());
            node_ = node_->next_[0]; 
        }

        void Prev();

        void Seek(const Key& key);

        void SeekToFirst();

        void SeekToLast();
     private:
        const SkipList* list_;
        Node* node_;
    };
 private:
    static const int KMaxHeight = 12;

    Node* NewNode(const Key& key, int height);

    int RandomHeight();

    int GetMaxHeight() const {
        return max_height_.load(std::memory_order_relaxed);
    }

    Node* FindGreaterOrEqual(const Key& key, Node** prev) const;

    Node* FindLess(const Key& key) const;

    Node* FindLast() const;
    
    Comparator cmp_;
    MemBuffer* mem_buffer_;
    Node* head_;
    std::atomic<int> max_height_;
    std::mt19937 rng_;
};

template<typename Key, class Comparator>
struct SkipList<Key,Comparator>::Node {
    explicit Node(const Key& key) : key_(key) {}
    Node* Next(int level) {
        assert(level >= 0);
        return next_[level].load(std::memory_order_acquire);
    }
    void SetNext(int level, Node* x) {
        assert(level >= 0);
        next_[level].store(x, std::memory_order_release);
    }
    Node* RelaxedNext(int level) {
        assert(level >= 0);
        return next_[level].load(std::memory_order_relaxed);
    }
    void RelaxedSetNext(int level, Node* x) {
        assert(level >= 0);
        next_[level].store(x, std::memory_order_relaxed);
    }
    Key const key_;
    std::atomic<Node*> next_[1];
};

template<typename Key, class Comparator>
SkipList<Key,Comparator>::SkipList(Comparator cmp ,MemBuffer* mem_buffer)
    : cmp_(cmp),mem_buffer_(mem_buffer),
      head_(NewNode(0, KMaxHeight)),
      max_height_(1), rng_(std::random_device{}()) {
    for (int i = 0; i < KMaxHeight; i++)
        head_->SetNext(i, nullptr);
} 

template<typename Key, class Comparator>
typename SkipList<Key,Comparator>::Node* 
SkipList<Key,Comparator>::NewNode(const Key& key, int height) {
    char* const node_memory = mem_buffer_->AllocateAlign(
        sizeof(Node) + sizeof(std::atomic<Node*>) * (height - 1)
    );
    return new (node_memory) Node(key);
}

template<typename Key, class Comparator>
int SkipList<Key,Comparator>::RandomHeight() {
    int height = 1;
    static const int KProbability = 4;
    while(height < KMaxHeight) {
        if((rng_() % KProbability) != 0)
            break;
        height++;
    }
    return height;
}
template<typename Key, class Comparator>
typename SkipList<Key,Comparator>::Node*  
SkipList<Key,Comparator>::FindGreaterOrEqual(const Key& key, Node** prev) const {
    Node* ret = head_;
    int height = GetMaxHeight() - 1;
    while(true) { 
        Node* next = ret->next_[height];
        if (next != nullptr && cmp_(key, next->key_) > 0) {
            ret = next;
        } else {
            if (prev != nullptr) prev[height] = ret;
            if (height == 0) return next;
            height--;
        }
    }
}

template<typename Key, class Comparator>
typename SkipList<Key,Comparator>::Node*  
SkipList<Key,Comparator>::FindLess(const Key& key) const {
    Node* ret = head_;
    int height = GetMaxHeight() - 1;
    while(true) { 
        Node* next = ret->next_[height];
        if (next != nullptr && cmp_(key, next->key_) < 0) {
            ret = next;
        } else {
            if (height == 0) return ret;
            height--;
        }
    }
}

template<typename Key, class Comparator>
typename SkipList<Key,Comparator>::Node*  
SkipList<Key,Comparator>::FindLast() const {
    Node* node = head_;
    int height = GetMaxHeight() - 1;
    while(true) {
        Node* next = node->Next(height);
        if (next != nullptr) {
            node = next;
        } else {
            if (height == 0) return node;
            height--;
        }
    }
}

template<typename Key, class Comparator>
void SkipList<Key,Comparator>::Insert(const Key& key) {
    Node* prev[KMaxHeight];
    Node* next = FindGreaterOrEqual(key, prev);

    int height = RandomHeight();
    Node* node = NewNode(key, height);

    if (height > GetMaxHeight()) {
        for (int i = GetMaxHeight(); i < height; i++) {
            prev[i] = head_;
        }
        max_height_.store(height,std::memory_order_relaxed);
    }
    // (1) RelaxedSetNext is ok, because the node hasn't been add to SkipList.
    //     Other users won't see the change.
    // (2) RelaxedNext is ok, "prev[i]->SetNext(i, node)"
    //     will make other users' "prev[i]->RelaxedNext(i)" see the change;
    for (int i = 0; i < height; i++) {
        node->RelaxedSetNext(i, prev[i]->RelaxedNext(i));
        prev[i]->SetNext(i, node);
    }
}

template<typename Key, class Comparator>
bool SkipList<Key,Comparator>::Contain(const Key& key) {
    Node* node = FindGreaterOrEqual(key, nullptr);
    return (node != nullptr) && cmp_(key, node->key_) == 0;
}

template<typename Key, class Comparator>
void SkipList<Key,Comparator>::Iterator::Prev() {
    assert(Valid());
    node_ = list_->FindLess(node_->key_);
    if (node_ == list_->head_) {
        node_ = nullptr;
    }
}

template<typename Key, class Comparator>
void SkipList<Key,Comparator>::Iterator::SeekToFirst() {
    node_ = list_->head_->Next(0);
}

template<typename Key, class Comparator>
void SkipList<Key,Comparator>::Iterator::Seek(const Key& key) {
    node_ = list_->FindGreaterOrEqual(key, nullptr);
}

template<typename Key, class Comparator>
void SkipList<Key,Comparator>::Iterator::SeekToLast() {
    node_ = list_->FindLast();
    if (node_ == list_->head_) {
        node_ = nullptr;
    }
}

}

#endif // STORAGE_XDB_DB_MEMTABLE_SKIPLIST_H_