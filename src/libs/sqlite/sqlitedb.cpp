#include "sqlitedb.hpp"

#include <sstream>
#include <sys/stat.h>

#include "../errnoexception.hpp"
#include "../scopeguard.hpp"
#include "sqliteexception.hpp"

namespace ctguard::libs::sqlite {

sqlite_db::sqlite_db(const std::string & db_path, bool create, mode_t mode) : m_db{ nullptr, &::sqlite3_close }
{
    int flag = SQLITE_OPEN_READWRITE;
    if (create) {
        flag |= SQLITE_OPEN_CREATE;
    }

    sqlite3 * tmp;
    const int ret = sqlite3_open_v2(db_path.c_str(), &tmp, flag, nullptr);
    m_db = std::unique_ptr<sqlite3, decltype(&::sqlite3_close)>{ tmp, &::sqlite3_close };

    if (ret) {
        throw sqlite_exception{ "Can not open database '" + db_path + "': " + error_msg() };
    }

    if (::chmod(db_path.c_str(), mode) == -1) {
        throw errno_exception{ "Can not chmod file '" + db_path + '\'' };
    }
}

}  // namespace ctguard::libs::sqlite
