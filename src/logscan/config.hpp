#pragma once

#include <string>
#include <vector>

namespace ctguard::logscan {

struct logfile_config
{
    std::string path;
    unsigned timeout_alert{ 0 };
};

enum class output_kind_t
{
    SOCKET,
    FILE
};

struct logscan_config
{
    std::vector<logfile_config> log_files;
    unsigned check_interval{ 1 };
    unsigned state_file_interval{ 30 };
    output_kind_t output_kind{ output_kind_t::SOCKET };
    std::string state_file{ "/var/lib/ctguard/logscan.state" };
    std::string output_path{ "/run/ctguard/research.sock" };
    std::string log_path{ "/var/log/ctguard/logscan.log" };
    std::string systemd_socket{ "/run/systemd/journal/syslog" };
    bool systemd_input{ true };
};

[[nodiscard]] logscan_config parse_config(const std::string & cfg_path);

std::ostream & operator<<(std::ostream & os, const logscan_config & cfg);
std::ostream & operator<<(std::ostream & os, output_kind_t ok);

} /* namespace ctguard::logscan */
