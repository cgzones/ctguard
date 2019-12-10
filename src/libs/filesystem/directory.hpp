#pragma once

#include <dirent.h>
#include <string>

#include "../errnoexception.hpp"

namespace ctguard::libs::filesystem {

class file_object
{
  public:
    using type_t = decltype(std::declval<struct ::dirent>().d_type);

    // cppcheck-suppress passedByValue
    file_object(type_t type, const char * name) noexcept : m_type{ type }, m_name{ name } {}
    file_object(const file_object & other) = delete;
    file_object & operator=(const file_object & other) = delete;
    file_object(file_object && other) = delete;
    file_object & operator=(file_object && other) = delete;

    [[nodiscard]] bool is_dir() const noexcept { return m_type == DT_DIR; }
    [[nodiscard]] bool is_reg() const noexcept { return m_type == DT_REG; }
    [[nodiscard]] bool is_lnk() const noexcept { return m_type == DT_LNK; }
    [[nodiscard]] bool is_dotordotdot() const noexcept { return ::strcmp(m_name, ".") == 0 || ::strcmp(m_name, "..") == 0; }
    [[nodiscard]] bool is_hidden() const noexcept { return m_name[0] == '.'; }
    [[nodiscard]] const char * name() const noexcept { return m_name; }

  private:
    type_t m_type;
    const char * m_name;
};

class child_iterator
{
  public:
    child_iterator(DIR * handle, struct ::dirent * entry) noexcept : m_handle{ handle }, m_entry{ entry } {}
    child_iterator(const child_iterator & other) noexcept : m_handle{ other.m_handle }, m_entry{ other.m_entry } {}

    child_iterator & operator=(const child_iterator & other) noexcept
    {
        m_handle = other.m_handle;
        m_entry = other.m_entry;
        return *this;
    }

    child_iterator & operator++()
    {
        m_entry = custom_readdir(m_handle);
        return *this;
    }

    [[nodiscard]] file_object operator*() const noexcept { return file_object{ m_entry->d_type, m_entry->d_name }; }

    [[nodiscard]] bool operator==(const child_iterator & other) const noexcept { return m_handle == other.m_handle && m_entry == other.m_entry; }
    [[nodiscard]] bool operator!=(const child_iterator & other) const noexcept { return !(*this == other); }

  private:
    // TODO(cgzones): propagate_const?
    DIR * m_handle;
    struct dirent * m_entry;

    friend class directory;

    [[nodiscard]] static struct ::dirent * custom_readdir(DIR * dir)
    {
        const auto safed_errno{ errno };
        errno = 0;

        // cppcheck-suppress readdirCalled  -- see readdir_r(3)
        struct ::dirent * ret{ ::readdir(dir) };
        if (ret == nullptr && errno != 0) {
            throw errno_exception{ "Can not readdir()" };
        }

        errno = safed_errno;
        return ret;
    }
};

class directory
{
  public:
    explicit directory(const std::string & path) : m_handle{ ::opendir(path.c_str()) }
    {
        if (!m_handle) {
            throw errno_exception{ "Can not open directory '" + path + "'" };
        }
    }
    ~directory() noexcept { ::closedir(m_handle); }
    directory(const directory & other) = delete;
    directory & operator=(const directory & other) = delete;
    directory(directory && other) = delete;
    directory & operator=(directory && other) = delete;

    child_iterator begin() { return child_iterator{ m_handle, child_iterator::custom_readdir(m_handle) }; }
    child_iterator end() const noexcept { return child_iterator{ m_handle, nullptr }; }

  private:
    // TODO(cgzones): propagate_const?
    DIR * m_handle;
};

} /* namespace ctguard::libs::filesystem */
