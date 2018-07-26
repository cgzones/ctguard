#pragma once

#include <dirent.h>
#include <string>

#include "../errnoexception.hpp"

namespace ctguard::libs::filesystem {

[[nodiscard]] static struct dirent * custom_readdir(DIR * dir);

class file_object
{
  public:
    file_object(unsigned char type, const char * name) : m_type{ type }, m_name{ name } {}

    [[nodiscard]] bool is_dir() const noexcept { return m_type == DT_DIR; }
    [[nodiscard]] bool is_lnk() const noexcept { return m_type == DT_LNK; }
    [[nodiscard]] const char * name() const noexcept { return m_name; }

  private:
    unsigned char m_type;
    const char * m_name;
};

class child_iterator
{
  public:
    child_iterator(DIR * handle, struct dirent * entry) : m_handle{ handle }, m_entry{ entry } {}
    child_iterator(const child_iterator & other) : m_handle{ other.m_handle }, m_entry{ other.m_entry } {}

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
    DIR * m_handle;
    struct dirent * m_entry;
};

class directory
{
  public:
    explicit directory(const std::string & path) : m_handle{ ::opendir(path.c_str()) }
    {
        if (!m_handle) {
            throw errno_exception{ "Can not open directory '" + path + '\'' };
        }
    }
    ~directory() noexcept { ::closedir(m_handle); }
    directory(const directory & other) = delete;
    directory & operator=(const directory & other) = delete;
    directory(directory && other) = delete;
    directory & operator=(directory && other) = delete;

    child_iterator begin() const { return child_iterator{ m_handle, custom_readdir(m_handle) }; }
    child_iterator end() const noexcept { return child_iterator{ m_handle, nullptr }; }

  private:
    DIR * m_handle;
};

inline struct dirent * custom_readdir(DIR * dir)
{
    errno = 0;
    struct dirent * ret = ::readdir(dir);
    if (ret == nullptr && errno != 0) {
        throw errno_exception{ "Can not readdir()" };
    }
    return ret;
}

} /* namespace ctguard::libs::filesystem */
