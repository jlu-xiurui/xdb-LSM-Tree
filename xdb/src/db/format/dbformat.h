#ifndef STORAGE_XDB_DB_FORMAT_DBFORMAT_H_
#define STORAGE_XDB_DB_FORMAT_DBFORMAT_H_

namespace xdb {

namespace config {
    static const int KNumLevels = 7;

    static const int KL0_CompactionThreshold = 4;

    static const int KL0_StopWriteThreshold = 12;

    static const uint64_t KMaxSequenceNumber = ((0x1ull << 56) - 1);
}

}

#endif // STORAGE_XDB_DB_FORMAT_DBFORMAT_H_
