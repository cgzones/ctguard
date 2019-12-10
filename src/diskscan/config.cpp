#include "config.hpp"

#include "../libs/config/parser.hpp"
#include "../libs/parsehelper.hpp"

#include "../libs/errnoexception.hpp"
#include <fstream>
#include <iomanip>

namespace ctguard::diskscan {

diskscan_config read_config(const std::string & cfg_path)
{
    diskscan_config cfg;

    std::ifstream cfg_file{ cfg_path };
    if (!cfg_file.is_open()) {
        throw libs::errno_exception{ "Can not open config file" };
    }

    libs::config::parser p{ cfg_file };
    const libs::config::config_group global_cfg = p.parse();

    bool found_top = false;
    for (const libs::config::config_group & top : global_cfg.subgroups()) {
        if (top.name() != "diskscan") {
            continue;
        }
        if (found_top) {
            throw std::out_of_range{ "Duplicate top level configgroup diskscan found at " + to_string(top.pos()) };
        }
        found_top = true;

        for (const auto & a : top) {
            if (a.first == "db_path") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.db_path = a.second.options[0];

            } else if (a.first == "diffdb_path") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                cfg.diffdb_path = a.second.options[0];

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

            } else if (a.first == "realtime_ignore") {
                if (a.second.options.size() != 1) {
                    throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                             " given: " + std::to_string(a.second.options.size()) };
                }
                try {
                    cfg.realtime_ignore = a.second.options[0];
                    std::smatch m;
                    std::string s{ "TEstXXThisshouldNotMatch" };
                    if (std::regex_search(s, m, cfg.realtime_ignore)) {
                        throw std::out_of_range{ "Invalid regex for configuration " + a.first + " given: matches everything" };
                    }
                } catch (const std::exception & e) {
                    throw std::out_of_range{ "Invalid regex for configuration " + a.first + " given: " + e.what() };
                }

            } else if (a.first == "scaninterval") {
                try {
                    cfg.scaninterval = libs::parse_second_duration(a.second.options);
                } catch (const std::exception & e) {
                    throw std::out_of_range{ "Invalid argument for configuration '" + a.first + "' given: " + e.what() };
                }

            } else if (a.first == "settle_time") {
                try {
                    cfg.settle_time = libs::parse_integral<uint16_t>(a.second.options[0]);
                } catch (const std::exception & e) {
                    throw std::out_of_range{ "Invalid argument for configuration '" + a.first + "' given: " + e.what() };
                }

            } else if (a.first == "block_size") {
                try {
                    cfg.block_size = libs::parse_integral<uint16_t>(a.second.options[0]);
                } catch (const std::exception & e) {
                    throw std::out_of_range{ "Invalid argument for configuration '" + a.first + "' given: " + e.what() };
                }

            } else if (a.first == "max_diff_size") {
                try {
                    cfg.max_diff_size = libs::parse_integral<uint32_t>(a.second.options[0]);
                } catch (const std::exception & e) {
                    throw std::out_of_range{ "Invalid argument for configuration '" + a.first + "' given: " + e.what() };
                }

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

            } else {
                throw std::out_of_range{ "Invalid configuration option '" + a.first + "' in scope diskscan" };
            }
        }
        for (const auto & b : top.subgroups()) {
            if (b.name() == "directory") {
                if (b.keyword().empty()) {
                    throw std::out_of_range{ "Empty key for configuration group '" + b.name() + "' at " + to_string(b.pos()) };
                }
                const libs::config::config_group & dir = b;
                bool recursive = true;
                bool realtime = false;
                bool check_diff = false;
                watch::scan_option option = watch::scan_option::FULL;

                for (const auto & a : dir) {
                    if (a.first == "recursive") {
                        if (a.second.options.size() != 1) {
                            throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                                     " given: " + std::to_string(a.second.options.size()) };
                        }
                        if (a.second.options[0] == "true") {
                            recursive = true;
                        } else if (a.second.options[0] == "false") {
                            recursive = false;
                        } else {
                            throw std::out_of_range{ "Invalid configuration value '" + a.second.options[0] + "' for element '" + a.first + "'" };
                        }

                    } else if (a.first == "realtime") {
                        if (a.second.options.size() != 1) {
                            throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                                     " given: " + std::to_string(a.second.options.size()) };
                        }
                        if (a.second.options[0] == "true") {
                            realtime = true;
                        } else if (a.second.options[0] == "false") {
                            realtime = false;
                        } else {
                            throw std::out_of_range{ "Invalid configuration value '" + a.second.options[0] + "' for element '" + a.first + "'" };
                        }

                    } else if (a.first == "checkdiff") {
                        if (a.second.options.size() != 1) {
                            throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                                     " given: " + std::to_string(a.second.options.size()) };
                        }
                        if (a.second.options[0] == "true") {
                            check_diff = true;
                        } else if (a.second.options[0] == "false") {
                            check_diff = false;
                        } else {
                            throw std::out_of_range{ "Invalid configuration value '" + a.second.options[0] + "' for element '" + a.first + "'" };
                        }

                    } else if (a.first == "checks") {
                        if (a.second.options.size() != 1) {
                            throw std::out_of_range{ "Invalid number of arguments for configuration " + a.first +
                                                     " given: " + std::to_string(a.second.options.size()) };
                        }
                        if (a.second.options[0] == "full") {
                            option = watch::scan_option::FULL;
                        } else {
                            throw std::out_of_range{ "Invalid configuration value '" + a.second.options[0] + "' for element '" + a.first + "'" };
                        }

                    } else {
                        throw std::out_of_range{ "Invalid configuration option '" + a.first + "' in scope directory" };
                    }
                }

                cfg.watches.emplace_back(dir.keyword(), option, recursive, realtime, check_diff);
            } else {
                throw std::out_of_range{ "Invalid configuration group '" + b.name() + "' in scope diskscan" };
            }
        }
    }

    if (!found_top) {
        throw std::out_of_range{ "No toplevel diskscan configuration group found" };
    }

    // TODO(cgzones): check recurive watch path is parent of other watch path

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

std::ostream & operator<<(std::ostream & os, const diskscan_config & cfg)
{
    os << "START config dump\n"
       << "    block_size          = " << cfg.block_size << " (entries)\n"
       << "    db_path             = " << cfg.db_path << "\n"
       << "    diffdb_path         = " << cfg.diffdb_path << "\n"
       << "    log_path            = " << cfg.log_path << "\n"
       << "    max_diff_size       = " << cfg.max_diff_size << " (bytes)\n"
       << "    output_kind         = " << cfg.output_kind << "\n"
       << "    output_path         = " << cfg.output_path << "\n"
       << "    realtime_ignore     = " << /*TODO:cfg.realtime_ignore*/ "TODO"
       << "\n"
       << "    scaninterval        = " << cfg.scaninterval << " second(s)\n"
       << "    settle_time         = " << cfg.settle_time << " millisecond(s)\n"
       << "    watches:\n";
    for (const auto & w : cfg.watches) {
        os << "        path: " << std::setw(20) << w.path() << ", recursive: " << std::boolalpha << w.recursive() << ", realtime: " << std::boolalpha
           << w.realtime() << ", options: " << w.option() << ", check_diff: " << w.check_diff() << "\n";
    }
    os << "END config dump\n";
    return os;
}

} /* namespace ctguard::diskscan */
