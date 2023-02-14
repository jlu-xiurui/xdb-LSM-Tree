#ifndef STORAGE_XDB_INCLUDE_FILTER_POLICY_H_
#define STORAGE_XDB_INCLUDE_FILTER_POLICY_H_

#include "include/slice.h"
namespace xdb {

class FilterPolicy {
 public:
    virtual ~FilterPolicy() = default;

    virtual const char* Name() const = 0;

    // keys[0..n-1] contains a list of keys.
    // the keys is used for create the new filter.
    // and the filter should be Append to dst (encoded into string)
    virtual void CreatFilter(const Slice* keys, int n, std::string* dst) const = 0;

    // the filter is created by "CreatFilter"
    // Return true : if key is potentially contained 
    // by "keys" of "CreatFilter".
    // Return false : if key must not be contained by keys.
    virtual bool KeyMayMatch(const Slice& key, const Slice& filter) const = 0;
};

FilterPolicy* NewBloomFilterPolicy(int bits_per_key);
}
#endif // STORAGE_XDB_INCLUDE_FILTER_POLICY_H_