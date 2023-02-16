#include <limits>

#include "util/filename.h"
#include "include/status.h"
#include "include/env.h"

namespace xdb { 
static std::string MakeFileName(const std::string& dbname, uint64_t number, const char* suffix) {
    char buf[100];
    std::snprintf(buf, sizeof(buf), "/%06llu.%s",
        static_cast<unsigned long long>(number), suffix);
    return dbname + buf;
}

std::string LogFileName(const std::string& dbname, uint64_t number) {
    return MakeFileName(dbname, number, "log");
}

std::string LockFileName(const std::string& dbname) {
    return dbname + "/LOCK";
}

bool ParseFilename(const std::string& filename, uint64_t* number, FileType* type) {
    Slice rest(filename);
    if (filename == "LOCK") {
        *number = 0;
        *type = KLockFile;
    } else {
        uint64_t num;
        if(!ParseNumder(&rest,&num)) {
            return false;
        }
        if(rest == ".log") {
            *type = KLogFile;
        } else {
            return false;
        }
        *number = num;
    }
    return true;
}
bool ParseNumder(Slice* input, uint64_t* num) {
    constexpr const uint64_t KUint64Max = std::numeric_limits<uint64_t>::max();
    constexpr const char KLastCharOfUint64Max =
        '0' + static_cast<char>(KUint64Max % 10);
    const uint8_t* start = reinterpret_cast<const uint8_t*>(input->data());
    const uint8_t* end = start + input->size();
    const uint8_t* p = start;
    uint64_t value = 0;
    for ( ;p != end; ++p) {
        uint8_t ch = *p;
        if (ch > '9' || ch < '0') break;
        if (value > KUint64Max / 10 ||
            (value == KUint64Max / 10 && ch > KLastCharOfUint64Max)) {
                return false;
        }
        value = value * 10 + (ch - '0');
    }
    *num = value;
    const size_t num_length = p - start;
    input->remove_prefix(num_length);
    return num_length != 0; 
}

std::string MetaFileName(const std::string& dbname, uint64_t number) {
    return MakeFileName(dbname, number, "meta");
}

std::string TmpFileName(const std::string& dbname, uint64_t number) {
    return MakeFileName(dbname, number, "tmp");
}

std::string CurrentFileName(const std::string& dbname) {
    return dbname + "/CURRENT";
}

Status SetCurrentFile(Env* env, const std::string& dbname, uint64_t number) {
    Slice meta_file_name = MetaFileName(dbname, number);
    meta_file_name.remove_prefix(dbname.size() + 1);
    std::string tmp = TmpFileName(dbname, number);

}

Status WriteStringToFileSync(Env* env, const Slice& str, const std::string& filename) {
    WritableFile* file;
    Status s = env->NewWritableFile(filename, &file);
    if (!s.ok()) {
        return s;
    }
    s = file->Append(str);
    if (s.ok()) {
        s = file->Sync();
    }
    if (s.ok()) {
        s = file->Close();
    }
    delete file;
    if (!s.ok()) {
        env->RemoveFile(filename);
    }
    return s;
    
}
}