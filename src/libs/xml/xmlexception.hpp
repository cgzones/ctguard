#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace ctguard::libs::xml {

class xml_exception : public std::exception
{
  public:
    explicit xml_exception(std::string_view msg);
    [[nodiscard]] virtual const char * what() const noexcept override;

  private:
    std::string m_msg;
};

} /* namespace ctguard::libs::xml */
