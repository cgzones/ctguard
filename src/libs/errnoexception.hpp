#pragma once

#include <exception>
#include <string>
#include <string_view>

namespace ctguard::libs {

class errno_exception : public std::exception
{
  public:
    explicit errno_exception(std::string_view message);
    [[nodiscard]] virtual const char * what() const noexcept override;

  private:
    std::string m_msg;
};

} /* namespace ctguard::libs */
