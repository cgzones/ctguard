#pragma once

#include <exception>
#include <string>
#include <string_view>

#include "position.hpp"

namespace ctguard::libs::config {

class parse_exception : public std::exception
{
  public:
    parse_exception(position pos, std::string_view message);

    [[nodiscard]] virtual const char * what() const noexcept override;

    [[nodiscard]] bool operator==(const parse_exception & other) const noexcept;

  private:
    position m_position;
    std::string m_message;
};

}  // namespace ctguard::libs::config
