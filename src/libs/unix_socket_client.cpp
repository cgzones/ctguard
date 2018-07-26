#include "unix_socket_client.hpp"

#include "errnoexception.hpp"
#include "libexception.hpp"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>  // ::close

namespace ctguard::libs {

constexpr int INVALID_SOCKET = -1;

unix_socket_client::unix_socket_client(std::string socket_path) : m_socket_path{ std::move(socket_path) }, m_socket{ INVALID_SOCKET }
{
    if (m_socket_path.length() > sizeof(std::declval<struct sockaddr_un>().sun_path) - 1) {
        throw libs::lib_exception{ "Socket path too long: " + std::to_string(m_socket_path.length()) + " / " +
                                   std::to_string(sizeof(std::declval<struct sockaddr_un>().sun_path) - 1) };
    }

    connect();
}

unix_socket_client::~unix_socket_client() noexcept
{
    close();
}

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

    struct sockaddr_un remote;
    remote.sun_family = AF_UNIX;
    ::strcpy(remote.sun_path, m_socket_path.c_str());
    const size_t len = ::strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (::connect(m_socket, reinterpret_cast<struct sockaddr *>(&remote), static_cast<unsigned>(len)) == -1) {
        throw libs::errno_exception{ "Can not connect to socket '" + m_socket_path + "'" };
    }
}

// https://beej.us/guide/bgnet/html/multi/advanced.html#sendall
static int sendall(int s, const char * buf, size_t * len) noexcept
{
    size_t total = 0;         // how many bytes we've sent
    size_t bytesleft = *len;  // how many we have left to send
    ssize_t n = 0;

    while (total < *len) {
        n = ::send(s, buf + total, bytesleft, 0);
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

}  // namespace ctguard::libs
