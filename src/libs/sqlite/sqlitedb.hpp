#pragma once

#include <memory>
#include <sqlite3.h>
#include <string>

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
    std::unique_ptr<sqlite3, decltype(&::sqlite3_close)> m_db;
};

}  // namespace ctguard::libs::sqlite
