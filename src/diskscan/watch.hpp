#pragma once

#include <iostream>
#include <string>

namespace ctguard::diskscan {

class watch
{
  public:
    enum class scan_option
    {
        FULL
    };

    watch(std::string path, scan_option option, bool recursive, bool realtime, bool check_diff)
      : m_path{ std::move(path) }, m_option{ option }, m_recursive{ recursive }, m_realtime{ realtime }, m_check_diff{ check_diff }
    {}

    [[nodiscard]] const std::string & path() const noexcept { return m_path; }
    [[nodiscard]] scan_option option() const noexcept { return m_option; }
    [[nodiscard]] bool recursive() const noexcept { return m_recursive; }
    [[nodiscard]] bool realtime() const noexcept { return m_realtime; }
    [[nodiscard]] bool check_diff() const noexcept { return m_check_diff; }

  private:
    std::string m_path;
    scan_option m_option;
    bool m_recursive, m_realtime, m_check_diff;
};

std::ostream & operator<<(std::ostream & os, watch::scan_option so);

}  // namespace ctguard::diskscan
