#ifndef STORAGE_XDB_UTIL_SLICE_H_
#define STORAGE_XDB_UTIL_SLICE_H_

#include <cassert>
#include <cstddef>
#include <string>
#include <cstring>
namespace xdb {
class Slice {
 public:

    Slice() : data_(""),size_(0) {}
    
    Slice(const char* data, size_t size) : data_(data), size_(size) {}
    
    Slice(const char* data) : data_(data), size_(strlen(data)) {}

    Slice(const std::string& s) : data_(s.data()), size_(s.size()) {}
 
    Slice(const Slice&) = default;

    Slice& operator=(const Slice&) = default;

    size_t size() const { return size_; }

    const char* data() const { return data_; }

    bool empty() const { return size_ == 0; }

    char operator[](size_t i) const {
        assert(size_ > i);
        return data_[i];
    }

    int compare(const Slice& b) const;

    void remove_prefix(size_t n) {
        assert(size_ >= n);
        data_ += n;
        size_ -= n;
    }

    void clear() {
        data_ = "";
        size_ = 0;
    }
    std::string ToString() const { return std::string(data_,size_); }
 private:
    const char* data_;
    size_t size_;
};

inline bool operator==(const Slice& a,const Slice& b){
    return (a.size() == b.size() && 
        memcmp(a.data(), b.data(), a.size()) == 0);
}

inline bool operator!=(const Slice& a,const Slice& b){
    return !(a == b);
}

inline int Slice::compare(const Slice& b) const {
    const size_t min_len = size_ < b.size() ? size_ : b.size();
    int r = memcmp(data_, b.data(), min_len);
    if (r == 0) {
        if (size_ < b.size()) 
            r = -1;
        else if (size_ > b.size()) 
            r = +1;
    }
    return r;
}
}
#endif // STORAGE_XDB_UTIL_SLICE_H_