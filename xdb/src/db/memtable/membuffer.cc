#include <cassert>

#include "db/memtable/membuffer.h"

namespace xdb {

    static const int kBlockSize = 4096;

    char* MemBuffer::NewBlock(size_t bytes) {
        char* block = new char[bytes];
        blocks_.push_back(block);
        memory_used_.fetch_add(bytes + sizeof(char*), std::memory_order_release);
        return block;
    }

    char* MemBuffer::Allocate(size_t bytes) {
        if (remain_size_ < bytes) {
            return AllocateFallBack(bytes);
        }
        char* result = buf_;
        buf_ += bytes;
        remain_size_ -= bytes;
        return result;
    }

    char* MemBuffer::AllocateAlign(size_t bytes) {
        const int align = (sizeof(void*) > 8) ? sizeof(void*) : 8;
        static_assert((align & (align - 1)) == 0
                ,"Pointer size should be a power of 2");
        size_t buf_low_bit = reinterpret_cast<size_t>(buf_) & (align - 1);
        size_t need = bytes + (align - buf_low_bit);
        char* result;
        if (need <= remain_size_) {
            result = buf_ + (align - buf_low_bit);
            buf_ += need;
            remain_size_ -= need;
        } else {
            result = AllocateFallBack(bytes);
        }
        assert((reinterpret_cast<size_t>(result) & (align - 1)) == 0);
        return result;
    }

    char* MemBuffer::AllocateFallBack(size_t bytes) {
        if (bytes > kBlockSize / 4 ) {
            // big block is directly allocated;
            return NewBlock(bytes);
        }
        // allocate a KBlockSize block and return part of block;
        // remain block used for membuffer.
        buf_ = NewBlock(kBlockSize);
        remain_size_ = kBlockSize;
        char* result = buf_;
        buf_ += bytes;
        remain_size_ -= bytes;
        return result;
    }       

}