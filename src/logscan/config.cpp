#include "config.hpp"

#include "../libs/config/parser.hpp"
#include "../libs/errnoexception.hpp"
#include "../libs/parsehelper.hpp"

#include <fstream>  // std::ifstream
#include <iomanip>  // std::setw

namespace ctguard::logscan {

logscan_config parse_config(const std::string & cfg_path)
{
    logscan_config cfg;

    std::ifstream cfg_file{ cfg_path };
    if (!cfg_file.is_open()) {
        throw libs::errno_exception{ "Can not open config file" };
    }

    libs::config::parser p{ cfg_file };
    const libs::config::config_group global_cfg = p.parse();

    bool found_top = false;
    for (const libs::config::config_group & top : global_cfg.subgroups()) {
        if (top.name() != "logscan") {
            continue;
        } else if (found_top) {
            throw std::out_of_range{ "Duplicate top level configgroup logscan found at " + to_string(top.pos()) };
        }
        found_top = true;

        for (const auto & a : top) {
            if (a.first == "check_interval") {
                try {
                    cfg.check_interval = libs::parse_second_duration(a.second.options);
                } catch (const std::exception & e) {
                    throw std::out_of_range{ "Invalid argument for configuration '" + a.first + "' given: " + e.what() };
                }

            } else if (a.first == "state_file_interval") {
                try {
                    cfg.check_interval = libs::parse_integral<unsigned int>(a.second.options[0]);
                } catch (const std::exception & e) {
                    throw std::out_of_range{ "Invalid argument for configuration '" + a.first + "' given: " + e.what() };
                }

            } else if (a.first == "state_file") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.state_file = a.second.options[0];

            } else if (a.first == "output_path") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.output_path = a.second.options[0];

            } else if (a.first == "log_path") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.log_path = a.second.options[0];

            } else if (a.first == "systemd_socket") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.systemd_socket = a.second.options[0];

            } else if (a.first == "output_kind") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }

                if (a.second.options[0] == "socket") {
                    cfg.output_kind = output_kind_t::SOCKET;
                } else if (a.second.options[0] == "file") {
                    cfg.output_kind = output_kind_t::FILE;
                } else {
                    throw std::out_of_range{ "Invalid argument for configuration " + a.first + " given: '" + a.second.options[0] + "'" };
                }

            } else if (a.first == "systemd_input") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                try {
                    cfg.systemd_input = libs::parse_bool(a.second.options[0]);
                } catch (const std::exception & e) {
                    throw std::out_of_range{ "Invalid argument '" + a.second.options[0] + "' for configuration " + a.first + " given: " + e.what() };
                }

            } else {
                throw std::out_of_range{ "Invalid configuration option '" + a.first + "' in group logscan at " + to_string(a.second.pos) };
            }
        }

        for (const libs::config::config_group & b : top.subgroups()) {
            if (b.name() == "logfile") {
                const libs::config::config_group & logfile = b;
                struct logfile_config lc;
                lc.path = logfile.keyword();
                if (b.keyword().empty()) {
                    throw std::out_of_range{ "Empty key for configuration group '" + b.name() + "' at " + to_string(b.pos()) };
                }
                for (const auto & c : logfile) {
                    if (c.first == "timeout_warning") {
                        try {
                            lc.timeout_alert = libs::parse_second_duration(c.second.options);
                        } catch (const std::exception & e) {
                            throw std::out_of_range{ "Invalid argument '" + c.second.options[0] + "' for configuration " + c.first + " given: " + e.what() };
                        }

                    } else {
                        throw std::out_of_range{ "Invalid configuration option '" + c.second.key + "' in group logfile at " + to_string(c.second.pos) };
                    }
                }

                cfg.log_files.emplace_back(lc);

            } else {
                throw std::out_of_range{ "Invalid configuration group '" + b.name() + "' at " + to_string(b.pos()) };
            }
        }
    }

    if (!found_top) {
        throw std::out_of_range{ "No toplevel logscan configuration group found" };
    }

    return cfg;
}

std::ostream & operator<<(std::ostream & os, output_kind_t ok)
{
    switch (ok) {
        case output_kind_t::SOCKET:
            os << "socket";
            break;
        case output_kind_t::FILE:
            os << "file";
            break;
    }

    return os;
}

std::ostream & operator<<(std::ostream & os, const logscan_config & cfg)
{
    os << "START config dump\n"
       << "    check_interval      = " << cfg.check_interval << " second(s)\n"
       << "    log_path            = " << cfg.log_path << "\n"
       << "    output_kind         = " << cfg.output_kind << "\n"
       << "    output_path         = " << cfg.output_path << "\n"
       << "    state_file          = " << cfg.state_file << "\n"
       << "    state_file_interval = " << cfg.state_file_interval << " (relative to check_interval)\n"
       << "    systemd_input       = " << std::boolalpha << cfg.systemd_input << "\n"
       << "    systemd_socket      = " << cfg.systemd_socket << "\n"
       << "    logfiles:\n";
    for (const auto & logfile : cfg.log_files) {
        os << "        path: " << std::setw(20) << logfile.path << ", timeout_warning: " << logfile.timeout_alert << " second(s)\n";
    }
    os << "END config dump\n";
    return os;
}

}  // namespace ctguard::logscan
