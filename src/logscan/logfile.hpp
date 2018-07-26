#pragma once

#include "config.hpp"

#include <ctime>
#include <exception>
#include <fstream>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace ctguard::logscan {

class logfile
{
  public:
    using pos_type = std::fstream::pos_type;
    using inode_type = decltype(std::declval<struct stat>().st_ino);
    using size_type = decltype(std::declval<struct stat>().st_size);

    explicit logfile(logfile_config config) : m_config{ std::move(config) } {}

    ~logfile() noexcept
    {
        if (m_fd != -1) {
            ::close(m_fd);
        }
    }

    logfile(const logfile &) = delete;
    logfile & operator=(const logfile &) = delete;
    logfile(logfile && other) noexcept
      : m_config{ std::move(other.m_config) }, m_fd{ other.m_fd },
        m_position{ other.m_position }, m_inode{ other.m_inode }, m_size{ other.m_size }, m_down{ other.m_down }, m_last_updated{ other.m_last_updated }
    {
        other.m_fd = -1;
    }
    logfile & operator=(logfile &&) = delete;

    void set_position(pos_type pos) noexcept { m_position = pos; }
    [[nodiscard]] pos_type get_position() const noexcept { return m_position; }

    int & fd() noexcept { return m_fd; }

    void set_inode(inode_type inode) noexcept { m_inode = inode; }
    [[nodiscard]] inode_type get_inode() const noexcept { return m_inode; }

    void set_size(size_type size) noexcept { m_size = size; }
    [[nodiscard]] size_type get_size() const noexcept { return m_size; }

    [[nodiscard]] const std::string & path() const { return m_config.path; }
    [[nodiscard]] const logfile_config & config() const { return m_config; }
    [[nodiscard]] std::time_t last_updated() const { return m_last_updated; }

    [[nodiscard]] bool down() const noexcept { return m_down; }
    void down(bool down) noexcept { m_down = down; }
    void update_time() { m_last_updated = std::time(nullptr); }
    void timeout_triggered(bool timeout_triggered) noexcept { m_timeout_triggered = timeout_triggered; }
    [[nodiscard]] bool timeout_triggered() const noexcept { return m_timeout_triggered; }

  private:
    logfile_config m_config;

    int m_fd{ -1 };

    pos_type m_position{ 0 };
    inode_type m_inode{ static_cast<inode_type>(-1) };
    size_type m_size{ -1 };

    bool m_down{ false };
    std::time_t m_last_updated{ 0 };
    bool m_timeout_triggered{ false };
};

}  // namespace ctguard::logscan
