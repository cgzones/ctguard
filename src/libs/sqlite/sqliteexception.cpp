#include "sqliteexception.hpp"

namespace ctguard::libs::sqlite {

sqlite_exception::sqlite_exception(std::string_view msg) : m_msg{ msg } {}

const char * sqlite_exception::what() const noexcept
{
    return m_msg.c_str();
}

} /* namespace ctguard::libs::sqlite */
