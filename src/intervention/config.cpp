#include "config.hpp"

#include "../libs/config/parser.hpp"
#include "../libs/errnoexception.hpp"
#include "../libs/parsehelper.hpp"

#include <fstream>  // std::ifstream
#include <regex>
#include <sstream>
#include <stdexcept>  // std::runtime_error
#include <string.h>   // strerror

namespace ctguard::intervention {

intervention_config parse_config(const std::string & cfg_path)
{
    intervention_config cfg;

    std::ifstream cfg_file{ cfg_path };
    if (!cfg_file.is_open()) {
        throw libs::errno_exception{ "Can not open config file" };
    }

    libs::config::parser p{ cfg_file };
    const libs::config::config_group global_cfg = p.parse();

    bool found_top = false;
    for (const libs::config::config_group & top : global_cfg.subgroups()) {
        if (top.name() != "intervention") {
            continue;
        } else if (found_top) {
            throw std::out_of_range{ "Duplicate top level configgroup research found at " + to_string(top.pos()) };
        }
        found_top = true;

        for (const auto & a : top) {
            if (a.first == "input_path") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.input_path = a.second.options[0];

            } else if (a.first == "log_path") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.log_path = a.second.options[0];

            } else {
                throw std::out_of_range{ "Invalid configuration option '" + a.first + "' in group research at " + to_string(a.second.pos) };
            }
        }

        for (const auto & b : top.subgroups()) {
            if (b.name() == "action") {
                if (b.keyword().empty()) {
                    throw std::out_of_range{ "Empty key for configuration group '" + b.name() + "' at " + to_string(b.pos()) };
                }

                intervention_action action;
                action.name = b.keyword();

                for (const auto & c : b) {
                    if (c.first == "command") {
                        if (c.second.options.size() != 1) {
                            throw std::out_of_range{ "Invalid number of arguments for configuration " + c.first +
                                                     " given: " + std::to_string(c.second.options.size()) };
                        }
                        action.command = c.second.options[0];

                    } else if (c.first == "user") {
                        if (c.second.options.size() != 1) {
                            throw std::out_of_range{ "Invalid number of arguments for configuration " + c.first +
                                                     " given: " + std::to_string(c.second.options.size()) };
                        }
                        action.user = c.second.options[0];

                    } else if (c.first == "group") {
                        if (c.second.options.size() != 1) {
                            throw std::out_of_range{ "Invalid number of arguments for configuration " + c.first +
                                                     " given: " + std::to_string(c.second.options.size()) };
                        }
                        action.group = c.second.options[0];

                    } else if (c.first == "command_timeout") {
                        try {
                            action.command_timeout = libs::parse_second_duration(c.second.options);
                        } catch (const std::exception & e) {
                            throw std::out_of_range{ "Invalid argument '" + c.second.options[0] + "' for configuration " + c.first + " given: " + e.what() };
                        }

                    } else if (c.first == "regex") {
                        if (c.second.options.size() != 1) {
                            throw std::out_of_range{ "Invalid number of arguments for configuration " + c.first +
                                                     " given: " + std::to_string(c.second.options.size()) };
                        }
                        action.regex = c.second.options[0];

                        try {
                            std::regex tmp{ action.regex };
                        } catch (const std::exception & e) {
                            throw std::out_of_range{ "Invalid regex '" + action.regex + "'given: " + e.what() };
                        }

                    } else if (c.first == "whitelist") {
                        action.whitelist = c.second.options;

                    } else {
                        throw std::out_of_range{ "Invalid configuration option '" + c.second.key + "' in group action at " + to_string(c.second.pos) };
                    }
                }

                if (action.command.empty()) {
                    throw std::out_of_range{ "No command given for action '" + action.name + "'" };
                }

                cfg.actions.emplace_back(std::move(action));

            } else {
                throw std::out_of_range{ "Invalid configuration group '" + b.name() + "' at " + to_string(b.pos()) };
            }
        }
    }
    if (!found_top) {
        throw std::out_of_range{ "No toplevel intervention configuration group found" };
    }

    return cfg;
}

static std::string dump_whitelist(const std::vector<std::string> & whitelist)
{
    std::ostringstream oss;
    for (const auto & wl : whitelist) {
        oss << wl << ", ";
    }
    return oss.str();
}

std::ostream & operator<<(std::ostream & out, const intervention_config & cfg)
{
    out << "START config dump\n";
    out << "    input_path:           " << cfg.input_path << "\n";
    out << "    log_path:             " << cfg.log_path << "\n";
    for (const auto & a : cfg.actions) {
        out << "    action:   name: " << a.name << "; cmd: " << a.command << "; user: " << a.user << "; group: " << a.group
            << "; command_timeout: " << a.command_timeout << "; regex: " << a.regex << "; whitelist: " << dump_whitelist(a.whitelist) << ";\n";
    }
    out << "END config dump\n";

    return out;
}

}  // namespace ctguard::intervention
