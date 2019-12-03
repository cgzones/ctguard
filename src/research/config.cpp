#include "config.hpp"

#include "../libs/config/parser.hpp"
#include "../libs/parsehelper.hpp"

#include "../libs/errnoexception.hpp"
#include <fstream>    // std::ifstream
#include <stdexcept>  // std::runtime_error
#include <string.h>   // strerror

namespace ctguard::research {

research_config parse_config(const std::string & cfg_path)
{
    research_config cfg;

    std::ifstream cfg_file{ cfg_path };
    if (!cfg_file.is_open()) {
        throw libs::errno_exception{ "Can not open config file '" + cfg_path + "'" };
    }

    libs::config::parser p{ cfg_file };
    const libs::config::config_group global_cfg = p.parse();

    bool found_top = false;
    for (const libs::config::config_group & top : global_cfg.subgroups()) {
        if (top.name() != "research") {
            continue;
        } else if (found_top) {
            throw std::out_of_range{ "Duplicate top level configgroup research found at " + to_string(top.pos()) };
        }
        found_top = true;

        for (const auto & a : top) {
            if (a.first == "rules_file") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.rules_file = a.second.options[0];

            } else if (a.first == "rules_directory") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.rules_directory = a.second.options[0];

            } else if (a.first == "log_path") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.log_path = a.second.options[0];

            } else if (a.first == "output_path") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.output_path = a.second.options[0];

            } else if (a.first == "input_path") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.input_path = a.second.options[0];
            }

            else if (a.first == "log_priority") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }

                try {
                    cfg.log_priority = libs::parse_integral<priority_t>(a.second.options[0]);
                } catch (std::exception & e) {
                    throw std::out_of_range{ "Invalid argument '" + a.second.options[0] + "' for configuration " + a.first + " given: " + e.what() };
                }

            } else if (a.first == "mail") {
                try {
                    cfg.mail = libs::parse_bool(a.second.options[0]);
                } catch (const std::exception & e) {
                    throw std::out_of_range{ "Invalid argument '" + a.second.options[0] + "' for configuration " + a.first + " given: " + e.what() };
                }

            } else if (a.first == "mail_interval") {
                try {
                    cfg.mail_interval = libs::parse_second_duration(a.second.options);
                } catch (const std::exception & e) {
                    throw std::out_of_range{ "Invalid argument '" + a.second.options[0] + "' for configuration " + a.first + " given: " + e.what() };
                }

            } else if (a.first == "mail_sample_time") {
                try {
                    cfg.mail_sample_time = libs::parse_second_duration(a.second.options);
                } catch (const std::exception & e) {
                    throw std::out_of_range{ "Invalid argument '" + a.second.options[0] + "' for configuration " + a.first + " given: " + e.what() };
                }

            } else if (a.first == "mail_priority") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }

                try {
                    cfg.mail_priority = libs::parse_integral<priority_t>(a.second.options[0]);
                } catch (std::exception & e) {
                    throw std::out_of_range{ "Invalid argument '" + a.second.options[0] + "' for configuration " + a.first + " given: " + e.what() };
                }

            } else if (a.first == "mail_max_sample_count") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }

                try {
                    cfg.mail_max_sample_count = libs::parse_integral<unsigned>(a.second.options[0]);
                } catch (std::exception & e) {
                    throw std::out_of_range{ "Invalid argument '" + a.second.options[0] + "' for configuration " + a.first + " given: " + e.what() };
                }

            } else if (a.first == "mail_instant") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }

                try {
                    cfg.mail_instant = libs::parse_integral<priority_t>(a.second.options[0]);
                } catch (std::exception & e) {
                    throw std::out_of_range{ "Invalid argument '" + a.second.options[0] + "' for configuration " + a.first + " given: " + e.what() };
                }

            } else if (a.first == "mail_port") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }

                try {
                    cfg.mail_port = libs::parse_integral<unsigned short>(a.second.options[0]);
                } catch (std::exception & e) {
                    throw std::out_of_range{ "Invalid argument '" + a.second.options[0] + "' for configuration " + a.first + " given: " + e.what() };
                }

            } else if (a.first == "mail_host") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.mail_host = a.second.options[0];

            } else if (a.first == "mail_fromaddr") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.mail_fromaddr = a.second.options[0];

            } else if (a.first == "mail_replyaddr") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.mail_replyaddr = a.second.options[0];

            } else if (a.first == "mail_toaddr") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.mail_toaddr = a.second.options[0];

            } else if (a.first == "intervention_path") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.intervention_path = a.second.options[0];

            } else if (a.first == "intervention_kind") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }

                if (a.second.options[0] == "socket") {
                    cfg.intervention_kind = intervention_kind_t::SOCKET;
                } else if (a.second.options[0] == "file") {
                    cfg.intervention_kind = intervention_kind_t::FILE;
                } else {
                    throw std::out_of_range{ "Invalid argument for configuration " + a.first + " given: '" + a.second.options[0] + "'" };
                }

            } else {
                throw std::out_of_range{ "Invalid configuration option '" + a.first + "' in group research at " + to_string(a.second.pos) };
            }
        }

        if (!top.subgroups().empty()) {
            const auto & b = top.subgroups().cbegin();
            throw std::out_of_range{ "Invalid configuration group '" + b->name() + "' at " + to_string(b->pos()) };
        }
    }
    if (!found_top) {
        throw std::out_of_range{ "No toplevel research configuration group found" };
    }

    return cfg;
}

std::ostream & operator<<(std::ostream & out, intervention_kind_t ik)
{
    switch (ik) {
        case intervention_kind_t::FILE:
            out << "file";
            break;
        case intervention_kind_t::SOCKET:
            out << "socket";
            break;
    }

    return out;
}

std::ostream & operator<<(std::ostream & out, const research_config & cfg)
{
    out << "START config dump\n"
        << "    input_path:           " << cfg.input_path << "\n"
        << "    intervention_kind     " << cfg.intervention_kind << "\n"
        << "    intervention_path     " << cfg.intervention_path << "\n"
        << "    log_path:             " << cfg.log_path << "\n"
        << "    log_priority:         " << cfg.log_priority << "\n"
        << "    mail_fromto           " << cfg.mail_fromaddr << "\n"
        << "    mail_host             " << cfg.mail_host << "\n"
        << "    mail_instant          " << cfg.mail_instant << "\n"
        << "    mail_interval         " << cfg.mail_interval << "\n"
        << "    mail_max_sample_count " << cfg.mail_max_sample_count << "\n"
        << "    mail_port             " << cfg.mail_port << "\n"
        << "    mail_priority         " << cfg.mail_priority << "\n"
        << "    mail_replyaddr        " << cfg.mail_replyaddr << "\n"
        << "    mail_sample_time      " << cfg.mail_sample_time << "\n"
        << "    mail_toaddr           " << cfg.mail_toaddr << "\n"
        << "    output_path:          " << cfg.output_path << "\n"
        << "    rules_directory:      " << cfg.rules_directory << "\n"
        << "    rules_file:           " << cfg.rules_file << "\n"
        << "END config dump\n";

    return out;
}

}  // namespace ctguard::research
