#pragma once

#include <string>
#include <vector>

namespace ctguard::intervention {

using timeout_t = unsigned int;

struct intervention_action
{
    std::string name;
    std::string command;
    std::string user;
    std::string group;

    std::string regex;
    std::vector<std::string> whitelist;

    timeout_t command_timeout{ 30 };  // 30 seconds
};

struct intervention_config
{
    std::string log_path{ "/var/log/ctguard/intervention.log" };
    std::string input_path{ "/run/ctguard_intervention.sock" };

    std::vector<intervention_action> actions;
};

[[nodiscard]] intervention_config parse_config(const std::string & cfg_path);

std::ostream & operator<<(std::ostream & out, const intervention_config & cfg);

}  // namespace ctguard::intervention
