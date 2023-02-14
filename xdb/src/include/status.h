#ifndef STORAGE_XDB_INCLUDE_STATUS_H_
#define STORAGE_XDB_INCLUDE_STATUS_H_

#include "include/slice.h"

namespace xdb {

class Status {
 public:
    Status() : state_(nullptr) {}

    ~Status() { delete[] state_; }

    Status(const Status& rhs);

    Status& operator=(const Status& rhs);

    Status& operator=(Status&& rhs) ;

    static Status OK() { return Status(); }

    static Status NotFound(const Slice& msg1, const Slice& msg2 = Slice()) {
        return Status(KNotFound, msg1, msg2);
    }
    static Status IOError(const Slice& msg1, const Slice& msg2 = Slice()) {
        return Status(KIOError, msg1, msg2);
    }
    static Status Corruption(const Slice& msg1, const Slice& msg2 = Slice()) {
        return Status(KCorruption, msg1, msg2);
    }
    bool IsNotFound() const{return code() == KNotFound; }
    
    bool IsIOError() const {return code() == KIOError; }

    bool IsCorruption() const {return code() == KCorruption; }
    
    bool ok() const { return state_ == nullptr; }

    std::string ToString() const;
 private:
    enum Code {
        KOK = 0,
        KNotFound = 1,
        KIOError = 2,
        KCorruption = 3
    };

    Code code() const {
        return (state_ == nullptr ? KOK : static_cast<Code>(state_[4]));
    }
    Status(Code code, const Slice& msg, const Slice& msg2);

    const char* state_;
};

const char* CopyState(const char* state);

inline Status& Status::operator=(const Status& rhs) {
    if(rhs.state_ != state_) {
        if(state_ != nullptr) {
            delete[] state_;
        }
        state_ = (rhs.state_ == nullptr ? nullptr : CopyState(rhs.state_));
    }
    return *this;
}

inline Status& Status::operator=(Status&& rhs) {
    std::swap(rhs.state_,state_);
    return *this;
}

inline Status::Status(const Status& rhs) {
    state_ = (rhs.state_ == nullptr ? nullptr : CopyState(rhs.state_));
}

}
#endif //STORAGE_XDB_INCLUDE_STATUS_H_