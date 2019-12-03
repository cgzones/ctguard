#pragma once

#include <string>

namespace ctguard::research {

using priority_t = unsigned short;

enum class intervention_kind_t
{
    SOCKET,
    FILE
};

struct research_config
{
    std::string rules_file{ "" };
    std::string rules_directory{ "/etc/ctguard/rules/" };
    std::string log_path{ "/var/log/ctguard/research.log" };
    std::string output_path{ "/var/log/ctguard/alerts.log" };
    std::string input_path{ "/run/ctguard/research.sock" };
    std::string intervention_path{ "/run/ctguard_intervention.sock" };
    intervention_kind_t intervention_kind{ intervention_kind_t::SOCKET };
    priority_t log_priority{ 1 };

    bool mail{ true };
    unsigned mail_interval{ 30 };
    unsigned mail_sample_time{ 1 };
    unsigned mail_max_sample_count{ 1000 };
    priority_t mail_priority{ 4 };
    priority_t mail_instant{ 7 };
    std::string mail_host{ "localhost" };
    std::string mail_fromaddr{ "ctguard@localhost" };
    std::string mail_toaddr{ "root@localhost" };
    std::string mail_replyaddr{ "root@localhost" };
    unsigned short mail_port{ 25 };
};

research_config parse_config(const std::string & cfg_file);

std::ostream & operator<<(std::ostream & out, const research_config & cfg);
std::ostream & operator<<(std::ostream & out, intervention_kind_t ik);

}  // namespace ctguard::research
