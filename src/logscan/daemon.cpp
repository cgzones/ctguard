#include "daemon.hpp"

#include "../libs/errnoexception.hpp"
#include "../libs/logger.hpp"
#include "../libs/source_event.hpp"
#include "eventsink.hpp"
#include "logfile.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stack>
#include <string.h>
#include <thread>

#include "../libs/blockedqueue.hpp"
#include "../libs/scopeguard.hpp"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>  // ::close

namespace ctguard::logscan {

using errorstack_t = std::pair<std::mutex, std::stack<std::exception_ptr>>;
using state_t = std::vector<std::tuple<std::string, logfile::inode_type, logfile::pos_type>>;

static std::atomic<bool> RUNNING{ true };
static bool UNIT_TEST{ false };

[[nodiscard]] static libs::source_event log_2_se(std::string source, bool control, std::string message) noexcept
{
    libs::source_event se;
    se.control_message = control;
    se.message = std::move(message);
    se.source_domain = std::move(source);
    se.source_program = "ctguard-logscan";
    if (!UNIT_TEST) {
        se.time_scanned = std::time(nullptr);
    }

    return se;
}

[[nodiscard]] static state_t get_state(const std::string & path)
{
    state_t state;
    std::ifstream state_file{ path };
    if (!state_file.is_open()) {
        if (errno == ENOENT) {
            FILE_LOG(libs::log_level::INFO) << "No statefile; clean start";
        } else {
            FILE_LOG(libs::log_level::ERROR) << "Can not open state file '" << path << "' : " << ::strerror(errno);
            FILE_LOG(libs::log_level::WARNING) << "Ignoring state file; clean start";
        }
    } else {
        std::string line;
        while (std::getline(state_file, line)) {
            if (line.empty()) {
                continue;
            }

            FILE_LOG(libs::log_level::DEBUG) << "State file line: '" << line << "'";
            const auto pathpos = line.find('"', 1);
            if (pathpos == line.npos) {
                FILE_LOG(libs::log_level::ERROR) << "State file '" << path << "' has invalid line: '" << line << "'";
                continue;
            }

            std::string file{ line.substr(1, pathpos - 1) };
            const std::string rest{ line.substr(pathpos + 2) };
            std::istringstream is{ rest };
            logfile::inode_type inode;
            unsigned long long pos;  // fpos<__mbstate_t> does not work in [is >>]

            is >> inode;
            if (is.fail() || is.get() != ',') {
                FILE_LOG(libs::log_level::ERROR) << "State file '" << path << "' has invalid node section: '" << rest << "'";
                continue;
            }

            is >> pos;
            if (is.fail()) {
                FILE_LOG(libs::log_level::ERROR) << "State file '" << path << "' has invalid position section: '" << rest << "'";
                continue;
            }

            FILE_LOG(libs::log_level::DEBUG) << "Extracted state: '" << file << "', '" << inode << "', '" << pos << "'";

            state.emplace_back(std::move(file), inode, pos);
        }
    }

    return state;
}

static void save_state(const std::string & path, const std::vector<logfile> & log_states)
{
    FILE_LOG(libs::log_level::DEBUG) << "Saving state...";
    {
        const std::string state_file_tmp = path + ".tmp";
        std::ofstream state_file{ state_file_tmp, std::ios::out | std::ios::trunc };
        if (!state_file.is_open()) {
            FILE_LOG(libs::log_level::ERROR) << "Can not open state file '" << state_file_tmp << "': " << ::strerror(errno);
            FILE_LOG(libs::log_level::WARNING) << "State not saved to disk";
        } else {
            for (const auto & ls : log_states) {
                state_file << "\"" << ls.path() << "\"," << ls.get_inode() << "," << ls.get_position() << "\n";
            }
            state_file.flush();

            state_file.close();

            if (::rename(state_file_tmp.c_str(), path.c_str()) == -1) {
                FILE_LOG(libs::log_level::ERROR) << "Can not rename '" << state_file_tmp << "' to '" << path << "': " << ::strerror(errno);
                FILE_LOG(libs::log_level::WARNING) << "State not saved to disk";
            }
        }
    }
    FILE_LOG(libs::log_level::DEBUG) << "State saved";
}

static void logfile_task(const logscan_config & cfg, libs::blocked_queue<libs::source_event> & queue, errorstack_t & es)
{
    FILE_LOG(libs::log_level::DEBUG) << "logfile-task started";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "logfile-task stopped"; } };

    try {
        std::vector<logfile> log_states;
        log_states.reserve(cfg.log_files.size());

        FILE_LOG(libs::log_level::DEBUG) << "Retrieving saved state...";
        const state_t saved_state{ get_state(cfg.state_file) };

        FILE_LOG(libs::log_level::DEBUG) << "Opening logfiles...";
        for (const logfile_config & lc : cfg.log_files) {
            const std::string & lf = lc.path;
            logfile state{ lc };

            state.fd() = ::open(lf.c_str(), O_RDONLY);
            if (state.fd() == -1) {
                if (errno == ENOENT) {
                    FILE_LOG(libs::log_level::WARNING) << "Can not open monitoring file '" << lf << "': " << ::strerror(errno);
                } else {
                    FILE_LOG(libs::log_level::ERROR) << "Can not open monitoring file '" << lf << "': " << ::strerror(errno);
                }
                state.down(true);
                log_states.emplace_back(std::move(state));
                continue;
            }

            struct stat tmp_stat;
            if (::fstat(state.fd(), &tmp_stat)) {
                FILE_LOG(libs::log_level::ERROR) << "Can not stat monitoring file '" << lf << "': " << ::strerror(errno);
                state.down(true);
                log_states.emplace_back(std::move(state));
                continue;
            }

            // check for saved state
            bool found_saved_state = false;
            for (const auto & sstate : saved_state) {
                if (std::get<0>(sstate) != state.path()) {
                    continue;
                }

                found_saved_state = true;

                const auto spos = std::get<2>(sstate);
                const auto sinode = std::get<1>(sstate);

                if (tmp_stat.st_ino != sinode) {
                    FILE_LOG(libs::log_level::INFO) << "Inode of file '" << lf << "' changed";
                    queue.emplace(log_2_se(lf, true, "!File got replaced"));
                    state.set_position(0);
                } else {
                    state.set_position(spos);
                }
            }

            state.set_inode(tmp_stat.st_ino);
            state.set_size(tmp_stat.st_size);
            if (!found_saved_state) {
                FILE_LOG(libs::log_level::INFO) << "No state for monitoring file '" << lf << "' found";
                state.set_position(tmp_stat.st_size);
            }
            state.update_time();

            log_states.emplace_back(std::move(state));
        }

        if (log_states.empty()) {
            FILE_LOG(libs::log_level::ERROR) << "No active file(s) to monitor. Exiting...";
            return;
        }

        FILE_LOG(libs::log_level::DEBUG) << "logfile scanner started";

        unsigned int state_file_interval{ 0 };
        while (RUNNING) {
            FILE_LOG(libs::log_level::DEBUG2) << "Loop begin...";

            for (auto & ls : log_states) {
                /* check for timeout warning */
                if (ls.config().timeout_alert != 0 && !ls.timeout_triggered()) {
                    if (std::time(nullptr) >= (ls.last_updated() + ls.config().timeout_alert)) {
                        FILE_LOG(libs::log_level::DEBUG) << "Timeout alert for file '" << ls.path() << "'";
                        ls.timeout_triggered(true);
                        queue.emplace(log_2_se(ls.path(), true, "!Timeout alert"));
                    }
                }

                if (ls.fd() == -1 || ls.down()) {
                    FILE_LOG(libs::log_level::DEBUG2) << "No stream: trying to set stream";
                    ls.fd() = ::open(ls.path().c_str(), O_RDONLY);
                    if (ls.fd() == -1) {
                        FILE_LOG(libs::log_level::DEBUG) << "Can still not open file '" << ls.path() << "': " << ::strerror(errno) << ". Skipping this one";
                        continue;
                    }
                    FILE_LOG(libs::log_level::INFO) << "File '" << ls.path() << "' coming alive";
                    queue.emplace(log_2_se(ls.path(), true, "!File coming alive"));
                    ls.down(false);
                    ls.timeout_triggered(false);
                }

                struct stat tmp_stat;
                if (::stat(ls.path().c_str(), &tmp_stat)) {
                    FILE_LOG(libs::log_level::ERROR) << "Can not stat file '" << ls.path() << "': " << ::strerror(errno);
                    queue.emplace(log_2_se(ls.path(), true, "!File went down"));
                    ls.down(true);
                    continue;
                }

                /* checking inode */
                if (tmp_stat.st_ino != ls.get_inode()) {
                    FILE_LOG(libs::log_level::INFO) << "Inode of file '" << ls.path() << "' changed";
                    queue.emplace(log_2_se(ls.path(), true, "!File got replaced"));

                    ls.set_inode(tmp_stat.st_ino);
                    ls.set_position(0);
                    FILE_LOG(libs::log_level::DEBUG) << "Try to reset stream";
                    ::close(ls.fd());
                    ls.fd() = ::open(ls.path().c_str(), O_RDONLY);
                    if (ls.fd() == -1) {
                        FILE_LOG(libs::log_level::ERROR) << "Can not open file '" << ls.path() << "': " << ::strerror(errno) << ". Skipping this one";
                        queue.emplace(log_2_se(ls.path(), true, "!File went down"));
                        continue;
                    }
                }

                /* checking size */
                if (tmp_stat.st_size == ls.get_size()) {
                    FILE_LOG(libs::log_level::DEBUG2) << "File '" << ls.path() << "' not changed. Early skip.";
                    continue;
                }

                if (tmp_stat.st_size < ls.get_size()) {
                    FILE_LOG(libs::log_level::INFO) << "File '" << ls.path() << "' truncated";
                    queue.emplace(log_2_se(ls.path(), true, "!File truncated"));

                    ls.set_position(0);
                }
                ls.set_size(tmp_stat.st_size);

                FILE_LOG(libs::log_level::DEBUG) << "Reading from stream...";
                std::ifstream st{ ls.path() };
                if (!st.is_open()) {
                    FILE_LOG(libs::log_level::ERROR) << "Can not open stream for file '" << ls.path() << "': " << ::strerror(errno);
                    continue;
                }

                st.seekg(ls.get_position());
                if (!st) {
                    FILE_LOG(libs::log_level::ERROR) << "Can not seek to last position (" << ls.get_position() << ") in file '" << ls.path()
                                                     << "': " << ::strerror(errno);
                    continue;
                }

                std::string line;
                while (st) {
                    std::getline(st, line);

                    if (!line.empty()) {
                        ls.set_position(st.tellg());

                        FILE_LOG(libs::log_level::DEBUG) << "Line got from '" << ls.path() << "': '" << line << "'";

                        ls.update_time();
                        ls.timeout_triggered(false);
                        queue.emplace(log_2_se(ls.path(), false, line));

                    } else {
                        FILE_LOG(libs::log_level::DEBUG) << "No input from '" << ls.path() << "'";
                    }
                }
                if (!st.eof()) {
                    FILE_LOG(libs::log_level::ERROR) << "File '" << ls.path() << "' not eof";
                }
                if (st.bad()) {
                    FILE_LOG(libs::log_level::ERROR) << "Stopped reading from file '" << ls.path() << "' due to bad";
                }
            }

            state_file_interval++;
            if (state_file_interval > cfg.state_file_interval) {
                save_state(cfg.state_file, log_states);
                state_file_interval = 0;
            }

            FILE_LOG(libs::log_level::DEBUG2) << "Waiting for " << cfg.check_interval << " seconds ...";
            sleep(cfg.check_interval);
        }

        save_state(cfg.state_file, log_states);

    } catch (...) {
        std::lock_guard lg{ es.first };
        es.second.push(std::current_exception());
    }
}

static void output_task(libs::blocked_queue<libs::source_event> & queue, eventsink & esink, errorstack_t & estack)
{
    FILE_LOG(libs::log_level::DEBUG) << "output-task started";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "output-task stopped"; } };

    std::queue<libs::source_event> local_queue;

    try {
        bool send_error_msg{ false };
        for (;;) {
            std::optional<libs::source_event> se{ queue.take(std::chrono::seconds(1)) };

            if (se.has_value()) {
                FILE_LOG(libs::log_level::DEBUG) << "output-task input: '" << se->message << "'";

                if (se->control_message && se->message == "!KILL") {
                    while (!local_queue.empty()) {
                        try {
                            esink.send(local_queue.front());
                            FILE_LOG(libs::log_level::DEBUG) << "output-task local queue element resend at shutdown";
                            local_queue.pop();
                        } catch (const std::exception & e) {
                            FILE_LOG(libs::log_level::ERROR) << "Can not forward message at shutdown: " << e.what();
                            break;
                        }
                    }

                    if (!local_queue.empty()) {
                        FILE_LOG(libs::log_level::WARNING) << "Discarding " << local_queue.size() << " events at shutdown due to sending errors";
                    }
                    return;
                }

                try {
                    esink.send(*se);
                    send_error_msg = false;
                } catch (const std::exception & e) {
                    if (!send_error_msg) {
                        FILE_LOG(libs::log_level::ERROR) << "Can not forward message in realtime: " << e.what();
                        send_error_msg = true;
                    }

                    local_queue.emplace(std::move(*se));
                }

            } else {
                FILE_LOG(libs::log_level::DEBUG2) << "output-task timeout";

                while (!local_queue.empty()) {
                    try {
                        esink.send(local_queue.front());
                        FILE_LOG(libs::log_level::DEBUG) << "output-task local queue element resend at timeout";
                        local_queue.pop();
                    } catch (const std::exception & e) {
                        FILE_LOG(libs::log_level::ERROR) << "Can not forward message at interval: " << e.what();
                        break;
                    }
                }
            }
        }

    } catch (...) {
        std::lock_guard lg{ estack.first };
        estack.second.push(std::current_exception());
    }
}

static void systemd_task(const logscan_config & cfg, libs::blocked_queue<libs::source_event> & queue, errorstack_t & es)
{
    FILE_LOG(libs::log_level::DEBUG) << "systemd-task started";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "systemd-task stopped"; } };

    if (!cfg.systemd_input) {
        FILE_LOG(libs::log_level::DEBUG) << "systemd input not enabled";
        return;
    }

    if (::geteuid() != 0) {
        FILE_LOG(libs::log_level::ERROR) << "systemd-task must be root for access permission";
        return;
    }

    try {
        int socket = ::socket(AF_UNIX, SOCK_DGRAM, 0);
        if (socket == -1) {
            throw libs::errno_exception{ "Can not create socket" };
        }
        libs::scope_guard close_socket{ [&socket]() { ::close(socket); } };

        struct sockaddr_un local;
        local.sun_family = AF_UNIX;
        constexpr size_t addr_length = sizeof(local.sun_path);
        if (cfg.systemd_socket.size() > (addr_length - 1)) {
            throw libs::errno_exception{ "Bind address to long: " + std::to_string(cfg.systemd_socket.size()) + "/" + std::to_string(addr_length - 1) };
        }
        const struct timeval tv
        {
            1, 0
        };  // 1s
        if (::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) == -1) {
            throw libs::errno_exception{ "Can not setsockopt()" };
        }
        ::strncpy(local.sun_path, cfg.systemd_socket.c_str(), addr_length - 1);
        const size_t len = strlen(local.sun_path) + sizeof(local.sun_family);
        if (::bind(socket, reinterpret_cast<struct sockaddr *>(&local), static_cast<unsigned>(len))) {
            throw libs::errno_exception{ "Can not bind on '" + cfg.systemd_socket + "'" };
        }
        libs::scope_guard unlink_socket{ [&cfg]() {
            if (::unlink(cfg.systemd_socket.c_str()) == -1) {
                FILE_LOG(libs::log_level::ERROR) << "Can not unlink socket '" << cfg.systemd_socket << "': " << ::strerror(errno);
            }
        } };

        while (RUNNING) {
            bool done = false;
            FILE_LOG(libs::log_level::DEBUG2) << "systemd-task waiting for connection...";

            do {
                std::array<char, 512> buffer;
                const ssize_t n = ::recv(socket, buffer.data(), buffer.size(), 0);
                if (n <= 0) {
                    if (n < 0) {
                        if (errno == EAGAIN) {
                            FILE_LOG(libs::log_level::DEBUG2) << "systemd-task Recv timeout";
                        } else if (errno == EINTR) {
                            FILE_LOG(libs::log_level::DEBUG) << "systemd-task Recv abort: " << ::strerror(errno);
                        } else {
                            FILE_LOG(libs::log_level::WARNING) << "systemd-task Recv error: " << ::strerror(errno);
                        }
                        break;
                    }
                    done = true;
                } else if (static_cast<size_t>(n) >= buffer.size() - 1) {
                    FILE_LOG(libs::log_level::WARNING) << "systemd-task Recv buffer to short: " << n << "/" << buffer.size();
                }

                if (static_cast<std::size_t>(n) >= buffer.size()) {
                    buffer[buffer.size() - 1] = '\0';
                } else {
                    buffer[static_cast<std::size_t>(n)] = '\0';
                }

                FILE_LOG(libs::log_level::DEBUG) << "systemd-task Recvieved (" << n << " bytes): '" << buffer.data() << "'";

                {
                    libs::source_event se;
                    if (buffer[0] == '<' && buffer[3] == '>') {
                        se.message = buffer.data() + 4;  // skip leading <12>
                    } else {
                        se.message = buffer.data();
                    }
                    se.source_domain = "systemd_syslog";
                    se.source_program = "ctguard-logscan";
                    se.time_scanned = std::time(nullptr);
                    queue.emplace(std::move(se));
                }

            } while (!done);
        }

    } catch (...) {
        std::lock_guard lg{ es.first };
        es.second.push(std::current_exception());
    }
}

void daemon(const logscan_config & cfg, bool unit_test)
{
    UNIT_TEST = unit_test;

    libs::blocked_queue<libs::source_event> event_queue;
    errorstack_t errorstack;

    eventsink es{ cfg.output_path, cfg.output_kind, UNIT_TEST };

    libs::scope_guard endmsg{ []() { FILE_LOG(libs::log_level::INFO) << "ctguard-logscan stopped"; } };

    FILE_LOG(libs::log_level::DEBUG) << "starting threads...";

    std::thread logfile_thread{ logfile_task, std::cref(cfg), std::ref(event_queue), std::ref(errorstack) };
    std::thread systemd_thread{ systemd_task, std::cref(cfg), std::ref(event_queue), std::ref(errorstack) };
    std::thread output_thread{ output_task, std::ref(event_queue), std::ref(es), std::ref(errorstack) };

    FILE_LOG(libs::log_level::DEBUG) << "threads started";

    sigset_t set;
    ::sigemptyset(&set);
    ::sigaddset(&set, SIGINT);
    ::sigaddset(&set, SIGTERM);
    ::sigprocmask(SIG_SETMASK, &set, nullptr);
    sigset_t signal_set;
    const struct timespec timeout
    {
        1, 0
    };  // 1s
    siginfo_t info;
    ::sigemptyset(&signal_set);
    ::sigaddset(&signal_set, SIGINT);
    ::sigaddset(&signal_set, SIGTERM);

    FILE_LOG(libs::log_level::INFO) << "ctguard-logscan started";
    event_queue.emplace(log_2_se("daemon", true, "ctguard-logscan started"));

    for (;;) {
        FILE_LOG(libs::log_level::DEBUG2) << "Waiting for signal or timeout...";
        if (sigtimedwait(&signal_set, &info, &timeout) < 0) {
            if (errno == EINTR) {
                /* other signal occurred */
                FILE_LOG(libs::log_level::WARNING) << "Unregistered signal occurred";
                continue;
            } else if (errno == EAGAIN) {
                /* Timeout, checking threads */
                FILE_LOG(libs::log_level::DEBUG2) << "Timeout";
                bool exc = false;
                std::lock_guard<std::mutex> lg{ errorstack.first };
                while (!errorstack.second.empty()) {
                    exc = true;
                    try {
                        std::exception_ptr exp{ errorstack.second.top() };
                        errorstack.second.pop();
                        std::rethrow_exception(exp);
                    } catch (const std::exception & e) {
                        FILE_LOG(libs::log_level::ERROR) << "Thread Exception: " << e.what();
                    }
                }
                if (exc) {
                    FILE_LOG(libs::log_level::ERROR) << "Exiting due to thread exception!";
                    break;
                }
                continue;
            } else {
                FILE_LOG(libs::log_level::ERROR) << "Error during sigtimedwait(): " << ::strerror(errno);
                break;
            }
        }

        /* requested signal occurred */
        FILE_LOG(libs::log_level::DEBUG) << "Registered signal found";
        break;
    }

    FILE_LOG(libs::log_level::INFO) << "ctguard-logscan shutting down...";
    event_queue.emplace(log_2_se("daemon", true, "ctguard-logscan shutting down..."));
    RUNNING = false;
    {
        libs::source_event se;
        se.control_message = true;
        se.message = "!KILL";
        event_queue.emplace(std::move(se));
    }

    FILE_LOG(libs::log_level::DEBUG) << "waiting for threads...";
    logfile_thread.join();
    systemd_thread.join();
    output_thread.join();
    FILE_LOG(libs::log_level::DEBUG) << "threads finished";
}

}  // namespace ctguard::logscan
