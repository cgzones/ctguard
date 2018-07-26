#include "unistd.h"
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <stack>
#include <string>
#include <sys/stat.h>
#include <thread>

#include "../libs/check_file_perms.hpp"
#include "../libs/config/parser.hpp"
#include "config.hpp"
#include "daemon.hpp"
#include "database.h"

#include "../libs/blockedqueue.hpp"
#include "../libs/errnoexception.hpp"
#include "../libs/filesystem/directory.hpp"
#include "../libs/logger.hpp"
#include "../libs/scopeguard.hpp"
#include "../libs/sqlite/sqlitestatement.hpp"
#include "filedata.hpp"
#include "watch.hpp"

using namespace ctguard::diskscan;
using namespace ctguard::libs;

static const char * default_cfg_path = "/etc/ctguard/diskscan.conf";
static const char * VERSION = "0.1dev";

static void print_help()
{
    std::cout << "usage:\n"
              << "  ctguard-diskscan [options]\n"
              << "    -v --verbose           print debug information (maybe specified multiple times)\n"
              << "    -f --foreground        run in foreground (useful for debugging and systemd)\n"
              << "    -c --cfg-file PATH     specify configuration file (default: " << default_cfg_path << ")\n"
              << "    -C --dump-config       print read configuration and exit\n"
              << "    -S --singlescan        run only a single scan and exit\n"
              << "    -x --unittest          precautions for unit tests (e.g. no timestamps, output to file)\n"
              << "    -V --version           print version and exit\n"
              << "    -h --help              this help overview\n";
}

int main(int argc, char ** argv)
{
    std::string cfg_path{ default_cfg_path };

    int verbose{ 0 };
    bool foreground{ false };
    bool configdump{ false };
    bool singlerun = false;
    bool unit_test{ false };

    while (1) {
        int option_index = 0;
        const struct option long_options[] = {
            { "verbose", no_argument, nullptr, 'v' },     { "foreground", no_argument, nullptr, 'f' }, { "cfg-file", required_argument, nullptr, 'c' },
            { "dump-config", no_argument, nullptr, 'C' }, { "singlescan", no_argument, nullptr, 'S' }, { "unittest", no_argument, nullptr, 'x' },
            { "version", no_argument, nullptr, 'V' },     { "help", no_argument, nullptr, 'h' },       { nullptr, 0, nullptr, 0 }
        };

        const int c = ::getopt_long(argc, argv, "vfc:CSxVh", long_options, &option_index);

        if (c == -1) {
            break;
        }

        switch (c) {
            case 0:
                if (long_options[option_index].flag != nullptr) {
                    break;
                }
                std::cerr << "option " << long_options[option_index].name;
                if (optarg)
                    std::cerr << " with arg " << optarg;
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

            case 'S':
                singlerun = true;
                break;

            case 'x':
                unit_test = true;
                break;

            case 'V':
                std::cout << "ctguard-diskscan " << VERSION << "\n";
                std::cerr << "ctguard-diskscan not started!\n";
                return EXIT_SUCCESS;

            case 'h':
                print_help();
                std::cerr << "ctguard-diskscan not started!\n";
                return EXIT_SUCCESS;

            case '?':
                /* getopt_long already printed an error message. */
                std::cerr << "ctguard-diskscan not started!\n";
                return EXIT_FAILURE;

            default:
                std::cerr << "ctguard-diskscan not started!\n";
                return EXIT_FAILURE;
        }
    }

    if (optind < argc) {
        std::cerr << "non-option arguments:\n";
        for (; optind < argc; ++optind) {
            std::cerr << "    " << argv[optind] << "\n";
        }
        std::cerr << "ctguard-diskscan not started!\n";
        return EXIT_FAILURE;
    }

    ::umask(0077);

    diskscan_config cfg = [&cfg_path]() {
        try {
            check_cfg_file_perms(cfg_path);
            return read_config(cfg_path);
        } catch (const std::exception & e) {
            std::cerr << "Can not parse configuration file '" << cfg_path << "': " << e.what() << "\n";
            std::cerr << "ctguard-diskscan not started!\n";
            exit(1);
        }
    }();

    if (configdump) {
        std::cout << cfg;
        std::cerr << "ctguard-diskscan not started!\n";
        return EXIT_SUCCESS;
    }

    FILELog::domain() = "ctguard-diskscan";
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
        std::cerr << "ctguard-diskscan not started!\n";
        return EXIT_FAILURE;
    }
    FILE_LOG(log_level::DEBUG) << "Logger initialized";
    ctguard::libs::scope_guard sg{ []() { FILE_LOG(log_level::DEBUG) << "Logger shutdown"; } };

    FILE_LOG(log_level::INFO) << "ctguard-diskscan starting (" << VERSION << ")...";
    try {
        daemon(cfg, singlerun, unit_test);
    } catch (const std::exception & e) {
        FILE_LOG(log_level::ERROR) << "Exiting due to error: " << e.what();
        return EXIT_FAILURE;
    } catch (...) {
        FILE_LOG(log_level::ERROR) << "Exiting due to unknown exception";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
