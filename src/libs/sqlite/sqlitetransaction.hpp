#pragma once

#include "sqlitedb.hpp"
#include "sqliteexception.hpp"

namespace ctguard::libs::sqlite {

class sqlite_transaction
{
  public:
    explicit sqlite_transaction(sqlite_db & db) : m_db{ db }
    {
        int ret;
        if ((ret = sqlite3_exec(m_db.m_db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr)) != SQLITE_OK) {
            std::ostringstream oss;
            oss << "Can not start transaction (" << ret << "): " << m_db.error_msg();
            throw sqlite_exception{ oss.str() };
        }
    }
    ~sqlite_transaction() noexcept(false)
    {
        if (!m_done) {
            try {
                fire_transaction();
            } catch (const sqlite_exception & e) {
                if (std::uncaught_exceptions() == 0) {
                    throw e;
                } else {
                    // we are in stackunwinding, do not throw
                }
            }
        }
    }

    void fire_transaction()
    {
        int ret;
        if ((ret = sqlite3_exec(m_db.m_db.get(), "END TRANSACTION", nullptr, nullptr, nullptr)) != SQLITE_OK) {
            std::ostringstream oss;
            oss << "Can not end transaction (" << ret << "): " << m_db.error_msg();
            throw sqlite_exception{ oss.str() };
        }
        m_done = true;
    }

  private:
    sqlite_db & m_db;
    bool m_done{ false };
};

}  // namespace ctguard::libs::sqlite
