#include "parseexception.hpp"

#include <sstream>

namespace ctguard::libs::config {

parse_exception::parse_exception(position pos, std::string_view message) : m_position{ pos }
{
    std::stringstream ss;
    ss << m_position.line_number() << ':' << m_position.line_indent() << ": " << message;
    m_message = ss.str();
}

const char * parse_exception::what() const noexcept
{
    return m_message.c_str();
}

bool parse_exception::operator==(const parse_exception & other) const noexcept
{
    return m_position == other.m_position && m_message == other.m_message;
}

} /* namespace ctguard::libs::config */
