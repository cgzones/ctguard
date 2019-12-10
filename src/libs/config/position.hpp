#pragma once

#include <ostream>

namespace ctguard::libs::config {

class position
{
  public:
    using line_t = unsigned int;
    using indent_t = unsigned int;

    position(line_t line_number, indent_t line_indent) : m_line_number{ line_number }, m_line_indent{ line_indent } {}

    [[nodiscard]] line_t line_number() const noexcept { return m_line_number; }
    [[nodiscard]] indent_t line_indent() const noexcept { return m_line_indent; }

    void line_break() noexcept;
    void char_advance() noexcept;

    bool operator==(const position & other) const noexcept;
    bool operator<(const position & other) const noexcept;

  private:
    line_t m_line_number;
    indent_t m_line_indent;
};

std::ostream & operator<<(std::ostream & out, const position & p);

std::string to_string(const position & p);

} /* namespace ctguard::libs::config */
