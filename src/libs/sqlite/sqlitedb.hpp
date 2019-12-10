#pragma once

#include <sqlite3.h>
#include <sys/stat.h>

#include <memory>

namespace ctguard::libs::sqlite {

class sqlite_db
{
  public:
    sqlite_db(const std::string & db_path, bool create, mode_t mode = 0600);

    [[nodiscard]] const char * error_msg() const noexcept { return ::sqlite3_errmsg(m_db.get()); }

    [[nodiscard]] bool operator==(const sqlite_db & other) const noexcept { return m_db == other.m_db; }

    friend class sqlite_statement;
    friend class sqlite_transaction;

  private:
    // TODO(cgzones): propagate_const
    std::unique_ptr<::sqlite3, decltype(&::sqlite3_close)> m_db;

    [[nodiscard]] ::sqlite3 * raw() noexcept { return m_db.get(); }
};

} /* namespace ctguard::libs::sqlite */
