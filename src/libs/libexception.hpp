#pragma once

#include <exception>
#include <string>

namespace ctguard::libs {

class lib_exception : public std::exception
{
  public:
    explicit lib_exception(std::string_view message);

    [[nodiscard]] virtual const char * what() const noexcept override;

  private:
    std::string m_message;
};

} /* namespace ctguard::libs */
