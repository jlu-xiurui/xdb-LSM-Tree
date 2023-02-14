#ifndef STORAGE_XDB_DB_MEMTABLE_MEMBUFFER_H_
#define STORAGE_XDB_DB_MEMTABLE_MEMBUFFER_H_

#include <vector>
#include <atomic>

namespace xdb {

class MemBuffer {
 public:
    MemBuffer() : 
        buf_(nullptr), remain_size_(0), memory_used_(0) {}
    ~MemBuffer() {
        for (char* block : blocks_)
            delete[] block;
    }

    // return a continous memorys of "bytes" byte
    // without align
    char* Allocate(size_t bytes);

    // return a continous memorys of "bytes" byte
    // with align. Used for structure with continous pointers.
    char* AllocateAlign(size_t bytes);

    // counter only need relaxed memory order
    size_t MemoryUsed() const {
        return memory_used_.load(std::memory_order_release);
    }
 private:
    char* AllocateFallBack(size_t bytes);
    
    char* NewBlock(size_t bytes);

    char* buf_;
    size_t remain_size_;
    std::vector<char*> blocks_;
    std::atomic<size_t> memory_used_;
};

}

#endif // STORAGE_XDB_DB_MEMTABLE_MEMBUFFER_H_