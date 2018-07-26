#pragma once

#include <string>
#include <string_view>

namespace ctguard::libs {

class unix_socket_client
{
  public:
    explicit unix_socket_client(std::string socket_path);
    unix_socket_client(const unix_socket_client & other) = delete;
    unix_socket_client & operator=(const unix_socket_client & other) = delete;
    unix_socket_client(unix_socket_client && other) = delete;
    unix_socket_client & operator=(unix_socket_client && other) = delete;
    ~unix_socket_client() noexcept;

    void send(std::string_view message);

  private:
    std::string m_socket_path;
    int m_socket;

    void connect();
    void close() noexcept;
};

}  // namespace ctguard::libs
