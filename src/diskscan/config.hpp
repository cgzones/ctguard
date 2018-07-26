#pragma once

#include <iostream>
#include <regex>
#include <vector>

#include "watch.hpp"

namespace ctguard::diskscan {

enum class output_kind_t
{
    SOCKET,
    FILE
};

struct diskscan_config
{
    std::vector<watch> watches;

    std::string db_path{ "/var/lib/ctguard/diskscan.db" };
    std::string diffdb_path{ "/var/lib/ctguard/diskscan_diff.db" };
    std::string log_path{ "/var/log/ctguard/diskscan.log" };
    std::string output_path{ "/run/ctguard/research.sock" };
    output_kind_t output_kind{ output_kind_t::SOCKET };

    std::regex realtime_ignore;

    uint64_t scaninterval{ 6 * 60 * 60 };  // 6h

    unsigned short settle_time{ 100 };

    unsigned short block_size{ 1000 };  // 1000 messages

    unsigned max_diff_size{ 1048576 };  // 1 MB
};

[[nodiscard]] diskscan_config read_config(const std::string & cfg_path);

std::ostream & operator<<(std::ostream & os, const diskscan_config & cfg);
std::ostream & operator<<(std::ostream & os, output_kind_t ok);

}  // namespace ctguard::diskscan
