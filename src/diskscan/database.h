#pragma once

#include <string>

#include "../libs/sqlite/sqlitedb.hpp"

namespace ctguard::diskscan {

class database
{
  public:
    explicit database(const std::string & path);

    operator ctguard::libs::sqlite::sqlite_db &() { return m_db; }  // TODO: delete

    bool empty();

  private:
    ctguard::libs::sqlite::sqlite_db m_db;
};

}  // namespace ctguard::diskscan
