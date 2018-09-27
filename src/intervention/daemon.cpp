#include "daemon.hpp"

#include "../libs/blockedqueue.hpp"
#include "../libs/errnoexception.hpp"
#include "../libs/intervention.hpp"
#include "../libs/libexception.hpp"
#include "../libs/logger.hpp"
#include "../libs/scopeguard.hpp"

#include <atomic>
#include <csignal>
#include <grp.h>
#include <mutex>
#include <pwd.h>
#include <regex>
#include <sstream>
#include <stack>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>  // ::close

#include <cereal/archives/binary.hpp>
#include <cereal/types/memory.hpp>

namespace ctguard::intervention {

using errorstack_t = std::pair<std::mutex, std::stack<std::exception_ptr>>;

static std::atomic<bool> RUNNING{ true };
static bool UNIT_TEST{ false };

static void run_cmd(const intervention_action & action, const std::string & argument)
{
    if (!UNIT_TEST) {
        if (::chdir("/") == -1) {
            throw libs::errno_exception{ "Can not chdir to '/'" };
        }
    }

    if (!action.group.empty()) {
        std::array<char, 4096> buffer;
        struct group gr;
        struct group * grp;
        const int ret = ::getgrnam_r(action.group.c_str(), &gr, buffer.data(), buffer.size(), &grp);
        if (ret != 0) {
            throw libs::errno_exception{ "Can not getgrnam_r for '" + action.group + "'" };
        }

        if (grp == nullptr) {
            throw libs::lib_exception{ "No such group found: " + action.group };
        }

        if (::setgid(grp->gr_gid) != 0) {
            throw libs::errno_exception{ "Can not setgid to gid " + std::to_string(grp->gr_gid) };
        }
    }

    if (!action.user.empty()) {
        std::array<char, 4096> buffer;
        struct passwd pw;
        struct passwd * pwd;
        const int ret = ::getpwnam_r(action.user.c_str(), &pw, buffer.data(), buffer.size(), &pwd);
        if (ret != 0) {
            throw libs::errno_exception{ "Can not getpwnam_r for '" + action.user + "'" };
        }

        if (pwd == nullptr) {
            throw libs::lib_exception{ "No such user found: " + action.user };
        }

        if (::setuid(pwd->pw_uid) != 0) {
            throw libs::errno_exception{ "Can not setuid to uid " + std::to_string(pwd->pw_uid) };
        }
    }

    char * const parmlist[] = { const_cast<char *>(action.command.c_str()), const_cast<char *>(argument.c_str()), nullptr };

    ::execv(action.command.c_str(), parmlist);

    throw libs::errno_exception{ "Can not exec '" + action.command + "'" };
}

static void processing_task(libs::blocked_queue<std::pair<libs::intervention_cmd, bool>> & input, const intervention_config & cfg, errorstack_t & es)
{
    FILE_LOG(libs::log_level::DEBUG) << "[pw] started.";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "[pw] stopped."; } };

    try {
        for (;;) {
            const auto ev_b{ input.take() };

            FILE_LOG(libs::log_level::DEBUG) << "[pw] input";

            if (ev_b.second) {
                return;
            }

            const libs::intervention_cmd & icmd{ ev_b.first };

            const intervention_action * matching_action = nullptr;
            for (const auto & action : cfg.actions) {
                if (action.name == icmd.name) {
                    matching_action = &action;
                    break;
                }
            }
            if (matching_action == nullptr) {
                FILE_LOG(libs::log_level::ERROR) << "No action found with name '" << icmd.name << "'";
                continue;
            }

            {
                try {
                    const std::regex r{ matching_action->regex };
                    std::smatch match;
                    if (!std::regex_match(icmd.argument, match, r)) {
                        FILE_LOG(libs::log_level::ERROR) << "Argument '" << icmd.argument << "' does not match regex '" << matching_action->regex
                                                         << "' for intervention '" << icmd.name << "'";
                        continue;
                    }
                } catch (const std::exception & e) {
                    FILE_LOG(libs::log_level::ERROR) << "Can not validate argument '" << icmd.argument << "' against regex '" << matching_action->regex
                                                     << "' for intervention '" << icmd.name << "': " << e.what();
                    continue;
                }
            }

            {
                bool found = false;
                for (const auto & wl : matching_action->whitelist) {
                    if (icmd.argument == wl) {
                        FILE_LOG(libs::log_level::INFO)
                          << "Not running intervention '" << icmd.name << "', cause argument '" << icmd.argument << "' is whitelisted";
                        found = true;
                        break;
                    }
                }

                if (found) {
                    continue;
                }
            }

            const int pid = ::fork();
            if (pid == -1) {
                throw libs::errno_exception{ "Can not fork" };
            } else if (pid == 0) {
                try {
                    run_cmd(*matching_action, icmd.argument);
                } catch (const std::exception & e) {
                    FILE_LOG(libs::log_level::ERROR) << "Can not run action '" << icmd.name << "' with argument '" << icmd.argument << "': " << e.what();
                    exit(EXIT_FAILURE);
                }
                FILE_LOG(libs::log_level::ERROR) << "Dead fallthrough";
                exit(EXIT_FAILURE);
            } else {
                timeout_t timeout{ 0 };
                for (;;) {
                    int status;
                    const int waitret = ::waitpid(pid, &status, WNOHANG);
                    if (waitret == -1) {
                        FILE_LOG(libs::log_level::ERROR) << "Can not waitpid(): " << ::strerror(errno);
                        if (::kill(pid, SIGKILL) == -1) {
                            FILE_LOG(libs::log_level::ERROR) << "Can not kill child with pid " << pid << " on waitpid error: " << ::strerror(errno);
                        }
                        break;
                    } else if (waitret == 0) {
                        if (timeout >= matching_action->command_timeout) {
                            FILE_LOG(libs::log_level::ERROR)
                              << "Action '" << icmd.name << "' with argument '" << icmd.argument << "' did not exit within timeout";
                            if (::kill(pid, SIGKILL) == -1) {
                                FILE_LOG(libs::log_level::ERROR) << "Can not kill child with pid " << pid << " on timeout: " << ::strerror(errno);
                            }
                            break;
                        }
                        timeout++;
                        sleep(1);
                    } else {
                        if (!WIFEXITED(status)) {
                            FILE_LOG(libs::log_level::ERROR) << "Action '" << icmd.name << "' with argument '" << icmd.argument << "' did not exit";
                        } else if (WEXITSTATUS(status) != 0) {
                            FILE_LOG(libs::log_level::ERROR) << "Action '" << icmd.name << "' with argument '" << icmd.argument << "' did exit with value "
                                                             << std::to_string(WEXITSTATUS(status));
                        } else {
                            FILE_LOG(libs::log_level::INFO) << "Action '" << icmd.name << "' with argument '" << icmd.argument << "' successfully executed";
                        }
                        break;
                    }
                }
            }
        }
    } catch (...) {
        std::lock_guard<std::mutex> lg{ es.first };
        es.second.push(std::current_exception());
    }
}

static void input_task(libs::blocked_queue<std::pair<libs::intervention_cmd, bool>> & output, const std::string & input_path, errorstack_t & es)
{
    FILE_LOG(libs::log_level::DEBUG) << "[iw] started.";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "[iw] stopped."; } };

    try {
        FILE_LOG(libs::log_level::DEBUG) << "[iw] initializating socket...";
        int socket = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (socket == -1) {
            throw libs::errno_exception{ "Can not create socket" };
        }

        auto close_socket = libs::scope_guard([socket]() { ::close(socket); });

        const struct timeval tv
        {
            1, 0
        };  // 1s
        ::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

        struct sockaddr_un local;
        local.sun_family = AF_UNIX;
        constexpr size_t addr_length = sizeof(local.sun_path);
        if (input_path.size() > (addr_length - 1)) {
            throw libs::errno_exception{ "Bind address to long: " + std::to_string(input_path.size()) + "/" + std::to_string(addr_length - 1) };
        }
        ::strncpy(local.sun_path, input_path.c_str(), addr_length - 1);
        size_t len = strlen(local.sun_path) + sizeof(local.sun_family);
        if (::bind(socket, reinterpret_cast<struct sockaddr *>(&local), static_cast<unsigned>(len))) {
            throw libs::errno_exception{ "Can not bind on '" + input_path + "'" };
        }

        libs::scope_guard unlink_socket{ [&input_path]() { ::unlink(input_path.c_str()); } };

        // TODO: fchmod?
        if (::chmod(input_path.c_str(), 0770) == -1) {
            throw libs::errno_exception{ "Can not chmod on '" + input_path + "'" };
        }

        FILE_LOG(libs::log_level::DEBUG) << "[iw] socket created";

        while (RUNNING) {
            bool done = false;
            FILE_LOG(libs::log_level::DEBUG2) << "[iw] waiting for connection...";

            do {
                std::array<char, 16384> buffer;
                ssize_t n = ::recv(socket, buffer.data(), buffer.size(), 0);
                if (n <= 0) {
                    if (n < 0) {
                        if (errno == EAGAIN) {
                            FILE_LOG(libs::log_level::DEBUG2) << "[iw] Recv timeout";
                        } else if (errno == EINTR) {
                            FILE_LOG(libs::log_level::DEBUG) << "[iw] Recv abort: " << ::strerror(errno);
                        } else {
                            FILE_LOG(libs::log_level::WARNING) << "[iw] Recv error: " << ::strerror(errno);
                        }
                        break;
                    }
                    done = true;
                } else if (static_cast<size_t>(n) >= buffer.size() - 1) {
                    FILE_LOG(libs::log_level::WARNING) << "[iw] Recv buffer to short: " << n << "/" << buffer.size();
                }

                if (static_cast<std::size_t>(n) >= buffer.size()) {
                    buffer[buffer.size() - 1] = '\0';
                } else {
                    buffer[static_cast<std::size_t>(n)] = '\0';
                }

                FILE_LOG(libs::log_level::DEBUG) << "[iw] Recvieved (" << n << " bytes)";

                libs::intervention_cmd icmd;
                std::stringstream oss;
                oss.write(&buffer.data()[0], n);
                try {
                    cereal::BinaryInputArchive iarchive(oss);
                    iarchive(icmd);
                    output.emplace(std::move(icmd), false);
                } catch (const std::exception & e) {
                    FILE_LOG(libs::log_level::ERROR) << "[iw] Can not decode message: " << e.what();
                }
            } while (!done);
        }

    } catch (...) {
        std::lock_guard<std::mutex> lg{ es.first };
        es.second.push(std::current_exception());
    }

    output.emplace(libs::intervention_cmd{}, true);
}

void daemon(const intervention_config & cfg, bool unit_test)
{
    UNIT_TEST = unit_test;
    libs::blocked_queue<std::pair<libs::intervention_cmd, bool>> input_queue;
    errorstack_t errorstack;

    auto endmsg = libs::scope_guard([]() { FILE_LOG(libs::log_level::INFO) << "intervention stopped"; });

    FILE_LOG(libs::log_level::DEBUG) << "starting threads...";

    auto input_thread = std::thread(input_task, std::ref(input_queue), std::cref(cfg.input_path), std::ref(errorstack));
    auto processing_thread = std::thread(processing_task, std::ref(input_queue), std::cref(cfg), std::ref(errorstack));

    FILE_LOG(libs::log_level::DEBUG) << "threads started";

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigprocmask(SIG_SETMASK, &set, nullptr);
    sigset_t signal_set;
    const struct timespec timeout
    {
        1, 0
    };  // 1s
    siginfo_t info;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);
    sigaddset(&signal_set, SIGTERM);
    FILE_LOG(libs::log_level::INFO) << "intervention started";

    for (;;) {
        FILE_LOG(libs::log_level::DEBUG2) << "Waiting for signal...";
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
                throw libs::errno_exception{ "Error during sigtimedwait()" };
            }
        }

        /* requested signal occurred */
        FILE_LOG(libs::log_level::DEBUG) << "Registered signal found";
        break;
    }

    FILE_LOG(libs::log_level::INFO) << "intervention shutting down...";
    RUNNING = false;

    FILE_LOG(libs::log_level::DEBUG) << "waiting for threads...";
    input_thread.join();
    processing_thread.join();
    FILE_LOG(libs::log_level::DEBUG) << "threads finished";
}

}  // namespace ctguard::intervention
