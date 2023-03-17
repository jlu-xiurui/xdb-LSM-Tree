#ifndef STORAGE_XDB_DB_INCLUDE_COMPARATOR_H_
#define STORAGE_XDB_DB_INCLUDE_COMPARATOR_H_

#include "include/slice.h"

namespace xdb {
class Comparator {
 public:
    virtual ~Comparator() = default;
 
    virtual int Compare(const Slice& a, const Slice& b) const = 0;

    virtual const char* Name() const = 0;
    // find the shortest string between with start and limit.
    virtual void FindShortestMiddle(std::string* start, const Slice& limit) const = 0;

    virtual void FindShortestBigger(std::string* start) const = 0;
};

const Comparator* DefaultComparator();

}

#endif // STORAGE_XDB_DB_INCLUDE_COMPARATOR_H_