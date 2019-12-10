#pragma once

#include "sqlitedb.hpp"
#include "sqliteexception.hpp"

namespace ctguard::libs::sqlite {

class sqlite_transaction
{
  public:
    explicit sqlite_transaction(sqlite_db & db) : m_db{ db }
    {
        if (const int ret = sqlite3_exec(m_db.m_db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr); ret != SQLITE_OK) {
            throw sqlite_exception{ "Can not start transaction (" + std::to_string(ret) + "): " + m_db.error_msg() };
        }
    }
    ~sqlite_transaction() noexcept(false)
    {
        if (m_done) {
            return;
        }

        try {
            fire_transaction();
        } catch (const sqlite_exception &) {
            // TODO: reconsider
            if (std::uncaught_exceptions() == 0) {
                // cppcheck-suppress exceptThrowInDestructor
                throw;  // throws e
            } else {
                // we are in stackunwinding, do not throw
            }
        }
    }

    void fire_transaction()
    {
        if (const int ret = sqlite3_exec(m_db.m_db.get(), "END TRANSACTION", nullptr, nullptr, nullptr); ret != SQLITE_OK) {
            throw sqlite_exception{ "Can not end transaction (" + std::to_string(ret) + "): " + m_db.error_msg() };
        }
        m_done = true;
    }

  private:
    sqlite_db & m_db;
    bool m_done{ false };
};

} /* namespace ctguard::libs::sqlite */
