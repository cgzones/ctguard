#pragma once

#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ctguard::libs::sqlite {

class sqlite_exception : public std::exception
{
  public:
    explicit sqlite_exception(std::string_view msg);
    [[nodiscard]] virtual const char * what() const noexcept override;

  private:
    std::string m_msg;
};

}  // namespace ctguard::libs::sqlite
