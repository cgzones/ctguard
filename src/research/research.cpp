#include "research.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "../libs/check_file_perms.hpp"
#include "../libs/config/parser.hpp"
#include "../libs/errnoexception.hpp"
#include "../libs/filesystem/directory.hpp"
#include "../libs/logger.hpp"
#include "../libs/scopeguard.hpp"
#include "../libs/source_event.hpp"

#include "config.hpp"
#include "daemon.hpp"
#include "event.hpp"
#include "process_log.hpp"
#include "rule.hpp"

using ctguard::libs::check_cfg_file_perms;
using ctguard::libs::FILELog;
using ctguard::libs::log_level;
using ctguard::libs::Output2FILE;
using ctguard::libs::source_event;
using ctguard::libs::filesystem::directory;
using ctguard::libs::filesystem::file_object;
using ctguard::research::event;
using ctguard::research::parse_config;
using ctguard::research::research_config;
using ctguard::research::rule_cfg;
using ctguard::research::rule_id_t;
using ctguard::research::rule_state;
using ctguard::research::RUNNING;
using ctguard::research::UNIT_TEST;

static const char * default_cfg_path = "/etc/ctguard/research.conf";
static const char * VERSION = "0.1dev";

static void print_help()
{
    std::cout << "usage:\n"
              << "  ctguard-research [options]\n"
              << "    -v --verbose           print debug information (may be specified multiple times)\n"
              << "    -f --foreground        run in foreground\n"
              << "    -c --cfg-file PATH     specify configuration file (default: " << default_cfg_path << ")\n"
              << "    -I --input             read input from stdin\n"
              << "    -C --config-dump       print read configuration and exit\n"
              << "    -x --unittest          precautions for unit tests (e.g. no timestamps)\n"
              << "    -V --version           print version and exit\n"
              << "    -h --help              this help overview\n";
}

int main(int argc, char ** argv)
{
    std::string cfg_path{ default_cfg_path };

    int verbose = 0;
    bool foreground = false;
    bool configdump = false;
    bool stdinput = false;

    while (true) {
        int option_index = 0;
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
        const struct option long_options[] = {
            { "verbose", no_argument, nullptr, 'v' }, { "foreground", no_argument, nullptr, 'f' },  { "cfg-file", required_argument, nullptr, 'c' },
            { "input", no_argument, nullptr, 'I' },   { "config-dump", no_argument, nullptr, 'C' }, { "unittest", no_argument, nullptr, 'x' },
            { "version", no_argument, nullptr, 'V' }, { "help", no_argument, nullptr, 'h' },        { nullptr, 0, nullptr, 0 }
        };

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
        const int c = ::getopt_long(argc, argv, "vfc:ICxVh", long_options, &option_index);

        if (c == -1) {
            break;
        }

        switch (c) {
            case 0:
                if (long_options[option_index].flag != nullptr) {  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    break;
                }
                std::cerr << "option " << long_options[option_index].name;  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                if (optarg) {
                    std::cerr << " with arg " << optarg;
                }
                std::cerr << "\n";
                break;

            case 'v':
                verbose++;
                break;

            case 'f':
                foreground = true;
                break;

            case 'c':
                cfg_path = optarg;
                break;

            case 'I':
                stdinput = true;
                break;

            case 'C':
                configdump = true;
                break;

            case 'x':
                UNIT_TEST = true;
                break;

            case 'V':
                std::cout << "ctguard-research " << VERSION << "\n";
                return EXIT_SUCCESS;

            case 'h':
                print_help();
                return EXIT_SUCCESS;

            case '?':
                /* getopt_long already printed an error message. */
            default:
                std::cerr << "ctguard-research not started!\n";
                return EXIT_FAILURE;
        }
    }

    if (optind < argc) {
        std::cerr << "non-option arguments:\n";
        for (; optind < argc; ++optind) {
            std::cerr << "    " << argv[optind] << "\n";  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
        std::cerr << "ctguard-research not started!\n";
        return EXIT_FAILURE;
    }

    ::umask(0077);

    // parse research config
    const research_config cfg = [&cfg_path]() {
        try {
            check_cfg_file_perms(cfg_path);
            return parse_config(cfg_path);
        } catch (const std::exception & e) {
            std::cerr << "Can not parse configuration file '" << cfg_path << "': " << e.what() << "\n";
            std::cerr << "ctguard-research not started!\n";
            exit(EXIT_FAILURE);
        }
    }();

    if (configdump) {
        std::cout << cfg;
        std::cerr << "ctguard-research not started!\n";
        return EXIT_SUCCESS;
    }

    // parse rule file
    if (cfg.rules_file.empty() && cfg.rules_directory.empty()) {
        std::cerr << "No rules file or directory set!\n";
        std::cerr << "ctguard-research not started!\n";
        return EXIT_FAILURE;
    }

    const rule_cfg rules = [&cfg]() {
        rule_cfg rules_tmp;
        if (!cfg.rules_file.empty()) {
            try {
                check_cfg_file_perms(cfg.rules_file);
                parse_rules(rules_tmp, cfg.rules_file);
            } catch (const std::exception & e) {
                std::cerr << "Can not parse rules file '" << cfg.rules_file << "': " << e.what() << "\n";
                std::cerr << "ctguard-research not started!\n";
                exit(EXIT_FAILURE);
            }
        } else {
            try {
                std::set<std::string> files;

                check_cfg_file_perms(cfg.rules_directory);

                directory dir{ cfg.rules_directory };

                for (const file_object e : dir) {
                    if (!e.is_reg()) {
                        continue;
                    }

                    if (e.is_hidden()) {
                        continue;
                    }

                    files.insert(cfg.rules_directory[cfg.rules_directory.length() - 1] == '/' ? cfg.rules_directory + e.name()
                                                                                              : cfg.rules_directory + '/' + e.name());
                }

                for (const auto & f : files) {
                    try {
                        check_cfg_file_perms(f);
                        parse_rules(rules_tmp, f);
                    } catch (const std::exception & e) {
                        std::cerr << "Can not parse rules file '" << f << "': " << e.what() << "\n";
                        std::cerr << "ctguard-research not started!\n";
                        exit(EXIT_FAILURE);
                    }
                }

            } catch (const std::exception & e) {
                std::cerr << "Can not parse rules directory '" << cfg.rules_directory << "': " << e.what() << "\n";
                std::cerr << "ctguard-research not started!\n";
                exit(EXIT_FAILURE);
            }
        }

        return rules_tmp;
    }();

    if (rules.std_rules.empty() && rules.group_rules.empty()) {
        std::cerr << "No rules loaded\n";
        std::cerr << "ctguard-research not started!\n";
        exit(EXIT_FAILURE);
    }

    // setup logger
    FILELog::domain() = "ctguard-research";
    FILELog::printprefix() = !foreground;
    switch (verbose) {
        case 0:
            FILELog::reporting_level() = log_level::INFO;
            break;
        case 1:
            FILELog::reporting_level() = log_level::DEBUG1;
            break;
        case 2:
            FILELog::reporting_level() = log_level::DEBUG2;
            break;
        case 3:
            FILELog::reporting_level() = log_level::DEBUG3;
            break;
        default:
            FILELog::reporting_level() = log_level::DEBUG4;
            break;
    }
    if (!(Output2FILE::stream() = (foreground ? stdout : ::fopen(cfg.log_path.c_str(), "ae")))) {
        std::cerr << "Can not open " << cfg.log_path << ": " << ::strerror(errno) << '\n';
        std::cerr << "ctguard-research not started!\n";
        return EXIT_FAILURE;
    }
    FILE_LOG(log_level::DEBUG) << "Logger initialized";
    ctguard::libs::scope_guard sg{ []() { FILE_LOG(log_level::DEBUG) << "Logger shutdown"; } };

    if (stdinput) {
        std::map<rule_id_t, struct rule_state> rules_state;
        std::cout << "ctguard-research\n\n"
                  << "  Enter loginput to see its handling.\n"
                  << "   - use 'quit' to exit\n";
        while (RUNNING) {
            std::string line;
            std::cout << "\n>";
            std::getline(std::cin, line);
            if (!RUNNING || line == "quit") {
                std::cout << "\nEnding\n";
                return EXIT_SUCCESS;
            }

            source_event se;
            se.message = std::move(line);
            const event & e = process_log(se, stdinput, rules, rules_state);
            std::cout << "\nRule matched:\n"
                      << "\trule id  : " << e.rule_id() << "\n"
                      << "\tpriority : " << e.priority() << "\n"
                      << "\tmessage  : " << (e.description().empty() ? "((none))" : e.description()) << "\n";
            if (e.priority() >= cfg.log_priority) {
                std::cout << "Would create alert with priority " << e.priority() << ".\n";
                for (const auto & intervention : e.interventions()) {
                    const auto it = e.fields().find(intervention.field);
                    if (it != e.fields().end()) {
                        std::cout << "Would fire intervention '" << intervention.name << "' with argument '" << it->second << "'.";
                    }
                }
            } else {
                std::cout << "Would not create any alert.\n";
            }
        }
    }

    /*if (UNIT_TEST) {
        std::map<rule_id_t, struct rule_state> rules_state;
        while (RUNNING) {
            std::string line;
            std::getline(std::cin, line);
            if (!RUNNING || line == "quit") {
                return EXIT_SUCCESS;
            }
            source_event se;
            se.message = std::move(line);
            const event & e = process_log(se, verbose, rules, rules_state);
            if (e.priority() >= cfg.log_priority) {
                std::cout << "ALERT START\n";
                std::cout << "Priority:  " << e.priority() << "\n";
                std::cout << "Info:      " << e.description() << " [" << e.rule_id() << "]\n";
                std::cout << "Log:       " << e.logstr() << "\n";
                if (!e.traits().empty()) {
                    std::cout << "Traits (" << e.traits().size() << "):\n";
                    for (const auto & iter : e.traits()) {
                        std::cout << "           " << iter.first << " : " << iter.second << "\n";
                    }
                }
                if (!e.fields().empty()) {
                    std::cout << "Extracted fields (" << e.fields().size() << "):\n";
                    for (const auto & iter : e.fields()) {
                        std::cout << "           " << iter.first << " : " << iter.second << "\n";
                    }
                }
                std::cout << "ALERT END\n\n";
            }
        }
    }*/

    FILE_LOG(log_level::INFO) << "research starting (" << VERSION << ")...";
    try {
        std::ofstream output_file{ cfg.output_path, std::ios::app };
        if (!output_file.is_open()) {
            throw std::invalid_argument{ "Can not open output file '" + cfg.output_path + "': " + ::strerror(errno) };
        }
        daemon(cfg, rules, output_file);
    } catch (const std::exception & e) {
        FILE_LOG(log_level::ERROR) << "Exiting due to error: " << e.what();
        return EXIT_FAILURE;
    } catch (...) {
        FILE_LOG(log_level::ERROR) << "Exiting due to unknown exception";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
