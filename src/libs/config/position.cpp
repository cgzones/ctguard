#include "position.hpp"

#include <sstream>

namespace ctguard::libs::config {

std::ostream & operator<<(std::ostream & out, const position & p)
{
    out << p.line_number() << ":" << p.line_indent();

    return out;
}

std::string to_string(const position & p)
{
    std::stringstream ss;
    ss << p;
    return ss.str();
}

void position::char_advance() noexcept
{
    m_line_indent++;
}

void position::line_break() noexcept
{
    m_line_number++;
    m_line_indent = 0;
}

bool position::operator==(const position & other) const noexcept
{
    return (m_line_number == other.m_line_number && m_line_indent == other.m_line_indent);
}

bool position::operator<(const position & other) const noexcept
{
    if (m_line_number == other.m_line_number) {
        return m_line_indent < other.m_line_indent;
    } else {
        return m_line_number < other.m_line_number;
    }
}

}  // namespace ctguard::libs::config
