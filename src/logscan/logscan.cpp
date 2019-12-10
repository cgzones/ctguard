#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "../libs/check_file_perms.hpp"
#include "../libs/logger.hpp"
#include "../libs/scopeguard.hpp"

#include "config.hpp"
#include "daemon.hpp"

using ctguard::libs::check_cfg_file_perms;
using ctguard::libs::FILELog;
using ctguard::libs::log_level;
using ctguard::libs::Output2FILE;
using ctguard::logscan::logscan_config;
using ctguard::logscan::parse_config;

static const char * default_cfg_path = "/etc/ctguard/logscan.conf";
static const char * VERSION = "0.1dev";

static void print_help()
{
    std::cout << "usage:\n"
              << "  ctguard-logscan [options]\n"
              << "    -v --verbose           print debug information (maybe specified multiple times)\n"
              << "    -f --foreground        run in foreground (useful for debugging and systemd)\n"
              << "    -c --cfg-file PATH     specify configuration file (default: " << default_cfg_path << ")\n"
              << "    -C --dump-config       print read configuration and exit\n"
              << "    -x --unittest          precautions for unit tests (e.g. no timestamps, output to file)\n"
              << "    -V --version           print version and exit\n"
              << "    -h --help              this help overview\n";
}

int main(int argc, char ** argv)
{
    std::string cfg_path{ default_cfg_path };

    int verbose = 0;
    bool foreground = false;
    bool configdump = false;
    bool unit_test = false;

    for (;;) {
        int option_index = 0;
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
        const struct ::option long_options[] = { { "verbose", no_argument, nullptr, 'v' },        { "foreground", no_argument, nullptr, 'f' },
                                                 { "cfg-file", required_argument, nullptr, 'c' }, { "dump-config", no_argument, nullptr, 'C' },
                                                 { "unittest", no_argument, nullptr, 'x' },       { "version", no_argument, nullptr, 'V' },
                                                 { "help", no_argument, nullptr, 'h' },           { nullptr, 0, nullptr, 0 } };

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
        const int c = ::getopt_long(argc, argv, "vfc:CxVh", long_options, &option_index);

        if (c == -1) {
            break;
        }

        switch (c) {
            case 0:
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                if (long_options[option_index].flag != nullptr) {
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

            case 'C':
                configdump = true;
                break;

            case 'x':
                unit_test = true;
                break;

            case 'V':
                std::cout << "ctguard-logscan " << VERSION << "\n";
                std::cerr << "ctguard-logscan not started!\n";
                return EXIT_SUCCESS;

            case 'h':
                print_help();
                std::cerr << "ctguard-logscan not started!\n";
                return EXIT_SUCCESS;

            case '?':
                /* getopt_long already printed an error message. */
            default:
                std::cerr << "ctguard-logscan not started!\n";
                return EXIT_FAILURE;
        }
    }

    if (optind < argc) {
        std::cerr << "non-option arguments:\n";
        for (; optind < argc; ++optind) {
            std::cerr << "    " << argv[optind] << "\n";  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
        std::cerr << "ctguard-logscan not started!\n";
        return EXIT_FAILURE;
    }

    ::umask(0077);

    const logscan_config cfg = [&cfg_path]() {
        try {
            check_cfg_file_perms(cfg_path);
            return parse_config(cfg_path);
        } catch (const std::exception & e) {
            std::cerr << "Can not parse configuration file '" << cfg_path << "': " << e.what() << "\n";
            std::cerr << "ctguard-logscan not started!\n";
            exit(1);
        }
    }();

    if (configdump) {
        std::cout << cfg;
        std::cerr << "ctguard-logscan not started!\n";
        return EXIT_SUCCESS;
    }

    if (cfg.log_files.empty() && !cfg.systemd_input) {
        std::cerr << "No file(s) to monitor.\n";
        std::cerr << "ctguard-logscan not started!\n";
        return EXIT_SUCCESS;
    }

    FILELog::domain() = "ctguard-logscan";
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
        std::cerr << "Can not open logfile '" << cfg.log_path << "': " << ::strerror(errno) << '\n';
        std::cerr << "ctguard-logscan not started!\n";
        return EXIT_FAILURE;
    }
    FILE_LOG(log_level::DEBUG) << "Logger initialized";
    ctguard::libs::scope_guard sg{ []() { FILE_LOG(log_level::DEBUG) << "Logger shutdown"; } };

    FILE_LOG(log_level::INFO) << "ctguard-logscan starting (" << VERSION << ")...";
    try {
        daemon(cfg, unit_test);
    } catch (const std::exception & e) {
        FILE_LOG(log_level::ERROR) << "Exiting due to error: " << e.what();
        return EXIT_FAILURE;
    } catch (...) {
        FILE_LOG(log_level::ERROR) << "Exiting due to unknown exception";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
