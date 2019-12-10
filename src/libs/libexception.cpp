#include "libexception.hpp"

namespace ctguard::libs {

lib_exception::lib_exception(std::string_view message) : m_message{ message } {}

const char * lib_exception::what() const noexcept
{
    return m_message.c_str();
}

} /* namespace ctguard::libs */
