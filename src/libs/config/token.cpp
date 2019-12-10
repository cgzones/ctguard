#include "token.hpp"

namespace ctguard::libs::config {

bool token::operator==(const std::string & other) const noexcept
{
    return m_content == other;
}

bool token::operator!=(const std::string & other) const noexcept
{
    return (!(*this == other));
}

bool token::operator==(char other) const noexcept
{
    return (m_content[0] == other && m_content[1] == '\0');
}

bool token::operator!=(char other) const noexcept
{
    return (!(*this == other));
}

bool token::operator==(const char * other) const noexcept
{
    return (m_content == other);
}
bool token::operator!=(const char * other) const noexcept
{
    return (!(*this == other));
}

} /* namespace ctguard::libs::config */
