#pragma once

#include <array>
#include <ostream>
#include <string>
#include <sys/stat.h>

#include "../libs/sqlite/sqlitedb.hpp"

#include "../external/sha2/sha2.h"

namespace ctguard::diskscan {

class file_data
{
  public:
    using size_type = decltype(std::declval<struct stat>().st_size);
    using inode_type = decltype(std::declval<struct stat>().st_ino);
    using mode_type = decltype(std::declval<struct stat>().st_mode);
    using mtime_type = decltype(std::declval<struct stat>().st_mtime);
    using ctime_type = decltype(std::declval<struct stat>().st_ctime);

    enum class type : unsigned int
    {
        directory,
        file,
        socket,
        link,
        block_device,
        character_device,
        fifo,
        unknown
    };

    [[nodiscard]] const std::string & path() const noexcept { return m_path; }
    [[nodiscard]] const std::string & user() const noexcept { return m_user; }
    [[nodiscard]] const std::string & group() const noexcept { return m_group; }
    [[nodiscard]] size_type size() const noexcept { return m_size; }
    [[nodiscard]] type get_type() const noexcept { return m_type; }
    [[nodiscard]] const std::string_view sha256() const noexcept { return std::string_view{ m_sha256.data(), m_sha256.size() - 1 }; }
    [[nodiscard]] bool exists() const noexcept { return m_exists; }
    [[nodiscard]] bool content_checked() const noexcept { return m_content_checked; }
    [[nodiscard]] const std::string & target() const noexcept { return m_target; }
    [[nodiscard]] bool dead_lnk() const noexcept { return m_dead_lnk; }
    [[nodiscard]] const std::string & diff() const noexcept { return m_diff; }

    [[nodiscard]] inode_type inode() const noexcept { return m_inode; }
    [[nodiscard]] mode_type mode() const noexcept { return m_mode; }
    [[nodiscard]] mtime_type mtime() const noexcept { return m_mtime; }
    [[nodiscard]] ctime_type ctime() const noexcept { return m_ctime; }

    friend std::ostream & operator<<(std::ostream & out, const file_data & fd);
    friend class file_data_factory;

  private:
    file_data() = default;
    bool m_exists = false;
    bool m_content_checked{ false };
    std::string m_path;
    std::string m_user;
    std::string m_group;
    size_type m_size = 0;
    type m_type = type::unknown;
    std::array<char, SHA256_DIGEST_STRING_LENGTH> m_sha256;
    std::string m_target;
    bool m_dead_lnk = false;
    inode_type m_inode = 0;
    mode_type m_mode = 0;
    mtime_type m_mtime = 0;
    ctime_type m_ctime = 0;
    std::string m_diff;
};

std::ostream & operator<<(std::ostream & out, const file_data::type t);

std::string print_mode(file_data::mode_type m);
std::string print_time(time_t t);

class diff_database
{
  public:
    explicit diff_database(const std::string & path);

    operator ctguard::libs::sqlite::sqlite_db &() { return m_db; }

  private:
    ctguard::libs::sqlite::sqlite_db m_db;
};

class file_data_factory
{
  public:
    file_data_factory(const std::string & db_path, unsigned max_diff_size) : m_db{ db_path }, m_max_diff_size{ max_diff_size } {}
    file_data construct(std::string path, bool check_content, bool check_diff);

  private:
    diff_database m_db;
    unsigned m_max_diff_size;
};

}  // namespace ctguard::diskscan
