#include "unix_socket_client.hpp"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>  // ::close

#include "errnoexception.hpp"
#include "libexception.hpp"

namespace ctguard::libs {

constexpr static int INVALID_SOCKET = -1;

unix_socket_client::unix_socket_client(std::string socket_path) : m_socket_path{ std::move(socket_path) }, m_socket{ INVALID_SOCKET }
{
    if (m_socket_path.length() > sizeof(std::declval<struct ::sockaddr_un>().sun_path) - 1) {
        throw libs::lib_exception{ "Socket path too long: " + std::to_string(m_socket_path.length()) + " / " +
                                   std::to_string(sizeof(std::declval<struct ::sockaddr_un>().sun_path) - 1) };
    }

    connect();
}

unix_socket_client::~unix_socket_client() noexcept
{
    close();
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void unix_socket_client::close() noexcept
{
    if (m_socket != INVALID_SOCKET) {
        ::close(m_socket);
    }
}

void unix_socket_client::connect()
{
    if ((m_socket = ::socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
        throw libs::errno_exception{ "Can not create socket" };
    }

    struct ::sockaddr_un remote;  // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
    remote.sun_family = AF_UNIX;
    ::strcpy(remote.sun_path, m_socket_path.c_str());  // NOLINT "length of m_socket_path is checked in constructor"
    const size_t len{ m_socket_path.length() + sizeof(remote.sun_family) };
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    if (::connect(m_socket, reinterpret_cast<struct ::sockaddr *>(&remote), static_cast<unsigned>(len)) == -1) {
        throw libs::errno_exception{ "Can not connect to socket '" + m_socket_path + "'" };
    }
}

// https://beej.us/guide/bgnet/html/multi/advanced.html#sendall
static int sendall(int s, const char * buf, std::size_t * len) noexcept
{
    size_t total = 0;         // how many bytes we've sent
    size_t bytesleft = *len;  // how many we have left to send
    ssize_t n = 0;

    while (total < *len) {
        n = ::send(s, buf + total, bytesleft, 0);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (n == -1) {
            break;
        }
        total += static_cast<unsigned>(n);
        bytesleft -= static_cast<unsigned>(n);
    }

    *len = total;  // return number actually sent here

    return n == -1 ? -1 : 0;  // return -1 on failure, 0 on success
}

void unix_socket_client::send(std::string_view message)
{
    std::size_t length = message.length();
    while (sendall(m_socket, message.data(), &length) == -1) {
        if (errno == ENOTCONN || errno == ECONNREFUSED) {
            close();
            connect();
        } else {
            throw errno_exception{ "Send error (" + std::to_string(length) + "/" + std::to_string(message.length()) + ")" };
        }
        // retry
        length = message.length();
    }
}

} /* namespace ctguard::libs */
