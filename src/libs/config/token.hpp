#pragma once

#include <ostream>
#include <string>

#include "position.hpp"

namespace ctguard::libs::config {

class token
{
  public:
    enum class type_t
    {
        eof,
        string,
        identifier,
        integer,
        unknown
    };

    token(token::type_t type, position position, std::string content) : m_type{ type }, m_position{ std::move(position) }, m_content{ std::move(content) } {}
    token(token::type_t type, position position, char content) : m_type{ type }, m_position{ std::move(position) }, m_content{ content } {}

    [[nodiscard]] token::type_t get_type() const noexcept { return m_type; }
    [[nodiscard]] const position & get_position() const noexcept { return m_position; }
    [[nodiscard]] const std::string & get_content() const noexcept { return m_content; }

    bool operator==(const std::string & other) const noexcept;
    bool operator==(char other) const noexcept;
    bool operator==(const char * other) const noexcept;
    bool operator!=(const std::string & other) const noexcept;
    bool operator!=(char other) const noexcept;
    bool operator!=(const char * other) const noexcept;

  private:
    type_t m_type;
    position m_position;
    std::string m_content;
};

}  // namespace ctguard::libs::config
