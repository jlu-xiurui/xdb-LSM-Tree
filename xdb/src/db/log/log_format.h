#ifndef STORAGE_XDB_DB_LOG_LOG_FORMAT_H_
#define STORAGE_XDB_DB_LOG_LOG_FORMAT_H_

#include <cstdint>

namespace xdb {
namespace log {

enum LogRecodeType{
    KZeroType = 0,
    KFullType = 1,
    KFirstType = 2,
    KMiddleType = 3,
    KLastType = 4
};

// 2 bytes max length

static const int KMaxType = 4;

static const int kBlockSize = 32768;

// 32bits crc | 16bits length | 8bits Type
static const int KLogHeadSize = 4 + 2 + 1;

}
}

#endif // STORAGE_XDB_DB_LOG_LOG_FORMAT_H_
