#include "errnoexception.hpp"

#include <cerrno>
#include <sstream>
#include <string.h>

namespace ctguard {
namespace libs {

errno_exception::errno_exception(std::string_view message)
{
    std::ostringstream oss;
    oss << message << ": " << ::strerror(errno);
    m_msg = oss.str();
}

const char * errno_exception::what() const noexcept
{
    return m_msg.c_str();
}

}  // namespace libs
}  // namespace ctguard
