#include "errnoexception.hpp"

#include <cerrno>
#include <cstring>

#include <array>
#include <sstream>

namespace ctguard::libs {

constexpr static std::size_t errormsg_size = 512;

errno_exception::errno_exception(std::string_view message)
{
    std::ostringstream oss;
    std::array<char, errormsg_size> error_buf;  // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)

    // use gnu strerror_r
    const char * res{ strerror_r(errno, error_buf.data(), error_buf.size() - 1) };
    if (res == nullptr) {
        oss << message << ": !Could not get string representation of " << errno << "!";
    } else {
        oss << message << ": " << res;
    }

    m_msg = oss.str();
}

const char * errno_exception::what() const noexcept
{
    return m_msg.c_str();
}

} /* namespace ctguard::libs */
