#include "daemon.hpp"

#include "event.hpp"
#include "intervention_sink.hpp"
#include "process_log.hpp"
#include "research.hpp"
#include "send_mail.hpp"

#include <mutex>
#include <sstream>
#include <stack>
#include <thread>

#include <csignal>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>  // ::close

#include "../libs/blockedqueue.hpp"
#include "../libs/errnoexception.hpp"
#include "../libs/logger.hpp"
#include "../libs/scopeguard.hpp"
#include <cereal/archives/binary.hpp>
#include <cereal/types/memory.hpp>

namespace ctguard::research {

using errorstack_t = std::pair<std::mutex, std::stack<std::exception_ptr>>;
struct intervention_t
{
    std::string name;
    std::string argument;
    bool kill;
};

static std::string raw_2_str(const char * str, std::size_t length)
{
    std::ostringstream oss;
    std::size_t i;
    for (i = 0; i < length; i++) {
        oss << "0x" << static_cast<unsigned int>(static_cast<unsigned char>(str[i]));
    }

    return oss.str();
}

static void processing_task(libs::blocked_queue<libs::source_event> & input, libs::blocked_queue<event> & alert_output,
                            libs::blocked_queue<intervention_t> & intervention_queue, const research_config & cfg, const rule_cfg & rules,
                            std::map<rule_id_t, struct rule_state> & rules_state, errorstack_t & es)
{
    FILE_LOG(libs::log_level::DEBUG) << "[pw] started.";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "[pw] stopped."; } };

    try {
        for (;;) {
            libs::source_event se{ input.take() };

            FILE_LOG(libs::log_level::DEBUG) << "[pw] input message: '" << se.message << "'";

            if (se.control_message && se.message == "!KILL") {
                event e{ se };
                alert_output.emplace(e);
                intervention_t tmp{ "", "", true };
                intervention_queue.emplace(tmp);
                return;
            }

            event e{ process_log(se, false, rules, rules_state) };

            if (e.priority() >= cfg.log_priority || e.always_alert()) {
                for (const auto & intervention : e.interventions()) {
                    if (e.fields().find(intervention.field) == e.fields().end()) {
                        if (!intervention.ignore_empty_field) {
                            FILE_LOG(libs::log_level::WARNING)
                              << "Empty argument '" << intervention.field << "' for intervention '" << intervention.name << "' by rule " << e.rule_id();
                        }
                    } else {
                        intervention_t tmp{ intervention.name, e.fields()[intervention.field], false };
                        intervention_queue.emplace(std::move(tmp));
                    }
                }

                alert_output.push(std::move(e));
                FILE_LOG(libs::log_level::DEBUG) << "[pw] event forwarded";
            } else {
                FILE_LOG(libs::log_level::DEBUG) << "[pw] event ignored due to too low priority";
            }
        }
    } catch (...) {
        std::lock_guard lg{ es.first };
        es.second.push(std::current_exception());
    }
}

static void state_task(const research_config & cfg, std::map<rule_id_t, struct rule_state> & rules_state, libs::blocked_queue<event> & output,
                       errorstack_t & es)
{
    FILE_LOG(libs::log_level::DEBUG) << "[st] started.";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "[st] stopped."; } };

    try {
        while (RUNNING) {
            FILE_LOG(libs::log_level::DEBUG2) << "[st] sleeping...";
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            for (auto & iter : rules_state) {
                std::lock_guard lg{ iter.second.mutex };

                if (iter.second.unless_triggered != 0) {
                    if (iter.second.unless_triggered + iter.second.unless_timeout < std::time(nullptr)) {
                        FILE_LOG(libs::log_level::DEBUG) << "[st] unless triggered...";
                        iter.second.unless_triggered = 0;

                        if (iter.second.unless_event.priority() >= cfg.log_priority || iter.second.unless_event.always_alert()) {
                            output.emplace(iter.second.unless_event);
                        }
                    }
                }
            }
        }

    } catch (...) {
        std::lock_guard lg{ es.first };
        es.second.push(std::current_exception());
    }
}

static void output_task(const research_config & cfg, libs::blocked_queue<event> & input, libs::blocked_queue<event> & mail_queue, std::ostream & output,
                        errorstack_t & es)
{
    FILE_LOG(libs::log_level::DEBUG) << "[ow] started.";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "[ow] stopped."; } };

    try {
        for (;;) {
            event e{ input.take() };

            FILE_LOG(libs::log_level::DEBUG1) << "[ow] event input: '" << e.logstr() << "'";

            if (e.control_message() && e.logstr() == "!KILL") {
                return;
            }

            const std::time_t t = std::time(nullptr);

            output << "ALERT START\n";
            if (!UNIT_TEST) {
                struct tm ts;
                ::localtime_r(&t, &ts);
                output << "Time:      " << std::put_time(&ts, "%c %Z\n");
            }
            output << "Priority:  " << e.priority() << "\n";
            output << "Info:      " << e.description() << " [" << e.rule_id() << "]\n";
            output << "Log:       " << e.logstr() << "\n";
            output << "Traits:\n";
            for (const auto & iter : e.traits()) {
                output << "           " << std::setw(20) << iter.first << " : " << iter.second << "\n";
            }
            if (!e.fields().empty()) {
                output << "Extracted fields:\n";
                for (const auto & iter : e.fields()) {
                    output << "           " << std::setw(20) << iter.first << " : " << iter.second << "\n";
                }
            }
            output << "ALERT END\n\n";
            output.flush();

            if (cfg.mail) {
                mail_queue.emplace(std::move(e));
            }
        }

    } catch (...) {
        std::lock_guard lg{ es.first };
        es.second.push(std::current_exception());
    }
}

static void intervention_task(const research_config & cfg, libs::blocked_queue<intervention_t> & intervention_queue, errorstack_t & es)
{
    FILE_LOG(libs::log_level::DEBUG) << "intervention-task started.";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "intervention-task stopped."; } };

    try {
        std::queue<libs::intervention_cmd> local_queue;
        intervention_sink isink{ cfg.intervention_path, cfg.intervention_kind };
        bool send_error_msg{ false };
        for (;;) {
            std::optional<intervention_t> intervention{ intervention_queue.take(std::chrono::seconds(1)) };

            if (intervention.has_value()) {
                FILE_LOG(libs::log_level::DEBUG) << "intervention-task input: '" << intervention->name << "', '" << intervention->argument << "'";

                if (intervention->kill) {
                    while (!local_queue.empty()) {
                        try {
                            isink.send(local_queue.front());
                            FILE_LOG(libs::log_level::DEBUG) << "intervention-task local queue element resend at shutdown";
                            local_queue.pop();
                        } catch (const std::exception & e) {
                            FILE_LOG(libs::log_level::ERROR) << "Can not forward intervention at shutdown: " << e.what();
                            break;
                        }
                    }

                    if (!local_queue.empty()) {
                        FILE_LOG(libs::log_level::WARNING) << "Discarding " << local_queue.size() << " interventions at shutdown due to sending errors";
                    }
                    return;
                }

                libs::intervention_cmd icmd{ std::move(intervention->name), std::move(intervention->argument) };
                try {
                    isink.send(icmd);
                    send_error_msg = false;
                } catch (const std::exception & e) {
                    if (!send_error_msg) {
                        FILE_LOG(libs::log_level::ERROR) << "Can not forward intervention in realtime: " << e.what();
                        send_error_msg = true;
                    }
                    local_queue.emplace(std::move(icmd));
                    sleep(1);
                }

            } else {
                FILE_LOG(libs::log_level::DEBUG2) << "intervention-task timeout";

                while (!local_queue.empty()) {
                    try {
                        isink.send(local_queue.front());
                        FILE_LOG(libs::log_level::DEBUG) << "intervention-task local queue element resend at timeout";
                        local_queue.pop();
                    } catch (const std::exception & e) {
                        FILE_LOG(libs::log_level::ERROR) << "Can not forward intervention at interval: " << e.what();
                        break;
                    }
                }
            }
        }

    } catch (...) {
        std::lock_guard lg{ es.first };
        es.second.push(std::current_exception());
    }
}

struct summary_item
{
    unsigned count;
    rule_id_t id;
    priority_t priority;
};

static void send_mail_queue(const research_config & cfg, const std::vector<event> & q)
{
    std::ostringstream mail, content;
    priority_t max_alert{ 0 };

    mail << "                        ######################\n"
         << "                        # ctguard alert mail #\n"
         << "                        #   " << std::setw(4) << q.size() << (q.size() == 1 ? " alert " : " alerts") << "      #\n"
         << "                        ######################\n\n";

    std::map<std::string, summary_item> summary_map;
    std::size_t interventions{ 0 };
    std::map<std::string, unsigned> interventions_details;

    for (const auto & ev : q) {
        summary_map[ev.description()].id = ev.rule_id();
        summary_map[ev.description()].priority = ev.priority();
        summary_map[ev.description()].count++;

        content << "#=========================== ALERT START ===========================#\n\n"
                << "  - Info:             " << ev.description() << " (id: " << ev.rule_id() << ")\n\n"
                << "  - Priority:        " << std::setw(2) << ev.priority() << "\n\n";

        if (!ev.interventions().empty()) {
            interventions += ev.interventions().size();
            content << "  - Intervention:\n";
            for (const auto & it : ev.interventions()) {
                interventions_details[it.name]++;
                const auto it_arg = ev.fields().find(it.field);
                if (it_arg != ev.fields().end()) {
                    content << "           " << std::setw(20) << it.name << " ( " << it.field << " :: " << it_arg->second << " )\n";
                } else {
                    content << "           " << std::setw(20) << it.name << " ( " << it.field << " :: !EMPTY! )\n";
                }
            }
        }

        content << "\n  - Log start\n    > " << ev.logstr() << "\n  - Log end\n\n"
                << "  - Traits:\n";

        for (const auto & iter : ev.traits()) {
            content << "           " << std::setw(20) << iter.first << " :: " << iter.second << "\n";
        }

        if (!ev.fields().empty()) {
            content << "  - Extracted fields:\n";
            for (const auto & iter : ev.fields()) {
                content << "           " << std::setw(20) << iter.first << " :: " << iter.second << "\n";
            }
        }

        content << "\n#============================ ALERT END ============================#\n\n";

        if (ev.priority() > max_alert) {
            max_alert = ev.priority();
        }
    }

    mail << "#============================= SUMMARY =============================#\n\n";

    for (const auto & iter : summary_map) {
        mail << "  " << std::setw(4) << iter.second.count << (iter.second.count == 1 ? " alert " : " alerts") << ": " << iter.first
             << " (id: " << iter.second.id << ") - Priority " << iter.second.priority << "\n";
    }
    if (interventions > 0) {
        mail << "  " << std::setw(4) << interventions << (interventions == 1 ? " intervention " : " interventions") << ":";
        bool first = true;
        for (const auto & iter : interventions_details) {
            if (first) {
                first = false;
            } else {
                mail << ",";
            }
            mail << " " << iter.second << " " << iter.first;
        }
        mail << "\n";
    }

    mail << "\n#============================= SUMMARY =============================#\n\n";

    mail << content.str();

    mail << "                        ######################\n"
         << "                        # ctguard alert mail #\n"
         << "                        #   " << std::setw(4) << q.size() << (q.size() == 1 ? " alert " : " alerts") << "      #\n"
         << "                        ######################\n\n";

    send_mail(cfg.mail_host, cfg.mail_port, cfg.mail_fromaddr, cfg.mail_toaddr,
              "ctguard :: " + std::to_string(q.size()) + " alert(s) :: max priority " + std::to_string(max_alert), cfg.mail_replyaddr, mail.str());
}

static void mail_task(const research_config & cfg, libs::blocked_queue<event> & mail_queue, errorstack_t & es)
{
    FILE_LOG(libs::log_level::DEBUG) << "[mq] started.";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "[mq] stopped."; } };

    try {
        std::vector<event> local_queue;
        priority_t max_priority{ 0 };
        std::time_t last_send{ 0 }, last_event{ 0 };
        while (RUNNING) {
            std::optional<event> opt_e{ mail_queue.take(std::chrono::seconds(1)) };

            if (opt_e.has_value()) {
                if (opt_e->priority() >= cfg.mail_priority || opt_e->always_alert()) {
                    FILE_LOG(libs::log_level::DEBUG1) << "[mq] event input: '" << opt_e->logstr() << "'";
                    if (opt_e->priority() > max_priority) {
                        max_priority = opt_e->priority();
                    }
                    local_queue.emplace_back(std::move(*opt_e));
                    last_event = std::time(nullptr);
                } else {
                    FILE_LOG(libs::log_level::DEBUG) << "[mq] ignoring mail evevnt";
                }
            }

            if (max_priority >= cfg.mail_instant ||
                (last_send + cfg.mail_interval <= std::time(nullptr) && last_event + cfg.mail_sample_time <= std::time(nullptr)) ||
                (local_queue.size() >= cfg.mail_max_sample_count && last_send < std::time(nullptr))) {
                if (!local_queue.empty()) {
                    try {
                        last_send = std::time(nullptr);
                        send_mail_queue(cfg, local_queue);
                        local_queue = std::vector<event>();
                        max_priority = 0;
                    } catch (const std::exception & e) {
                        FILE_LOG(libs::log_level::ERROR) << "Can not send alert mail: " << e.what();
                    }
                }
            }
        }

        if (!local_queue.empty()) {
            try {
                send_mail_queue(cfg, local_queue);
                local_queue = std::vector<event>();
            } catch (const std::exception & e) {
                FILE_LOG(libs::log_level::ERROR) << "Can not send alert mail: " << e.what();
            }
        }

        if (!local_queue.empty()) {
            FILE_LOG(libs::log_level::WARNING) << "Discarding " << local_queue.size() << " mail alerts at shutdown due to sending errors";
        }

    } catch (...) {
        std::lock_guard<std::mutex> lg{ es.first };
        es.second.push(std::current_exception());
    }
}

static void input_task(libs::blocked_queue<libs::source_event> & output, const std::string & input_path, errorstack_t & es)
{
    FILE_LOG(libs::log_level::DEBUG) << "[iw] started.";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "[iw] stopped."; } };

    try {
        FILE_LOG(libs::log_level::DEBUG) << "[iw] initializating socket...";
        int socket = ::socket(AF_UNIX, SOCK_DGRAM, 0);
        if (socket == -1) {
            throw libs::errno_exception{ "Can not create socket" };
        }

        libs::scope_guard close_socket{ [socket]() { ::close(socket); } };

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

                FILE_LOG(libs::log_level::DEBUG) << "[iw] Recvieved (" << n << " bytes): '" << raw_2_str(&buffer.data()[0], static_cast<std::size_t>(n)) << "'";

                libs::source_event se;
                std::stringstream oss;
                oss.write(&buffer.data()[0], n);
                try {
                    cereal::BinaryInputArchive iarchive(oss);
                    iarchive(se);
                    if (se.control_message && se.message == "!KILL") {
                        FILE_LOG(libs::log_level::WARNING) << "[iw] Ignoring external kill message";
                    } else {
                        output.emplace(se);
                    }
                } catch (const std::exception & e) {
                    FILE_LOG(libs::log_level::ERROR) << "[iw] Can not decode message: " << e.what();
                }
            } while (!done);
        }

    } catch (...) {
        std::lock_guard lg{ es.first };
        es.second.push(std::current_exception());
    }

    {
        libs::source_event se;
        se.control_message = true;
        se.message = "!KILL";
        output.emplace(std::move(se));
    }
}

void daemon(const research_config & cfg, const rule_cfg & rules, std::ostream & output)
{
    libs::blocked_queue<libs::source_event> input_queue;
    libs::blocked_queue<event> output_queue, mail_queue;
    libs::blocked_queue<intervention_t> intervention_queue;
    errorstack_t errorstack;
    std::map<rule_id_t, struct rule_state> rules_state;

    libs::scope_guard endmsg{ []() { FILE_LOG(libs::log_level::INFO) << "research stopped"; } };

    FILE_LOG(libs::log_level::DEBUG) << "starting threads...";

    auto input_thread = std::thread(input_task, std::ref(input_queue), std::cref(cfg.input_path), std::ref(errorstack));
    auto processing_thread = std::thread(processing_task, std::ref(input_queue), std::ref(output_queue), std::ref(intervention_queue), std::cref(cfg),
                                         std::cref(rules), std::ref(rules_state), std::ref(errorstack));
    auto state_thread = std::thread(state_task, std::cref(cfg), std::ref(rules_state), std::ref(output_queue), std::ref(errorstack));
    auto output_thread = std::thread(output_task, std::cref(cfg), std::ref(output_queue), std::ref(mail_queue), std::ref(output), std::ref(errorstack));
    auto intervention_thread = std::thread(intervention_task, std::cref(cfg), std::ref(intervention_queue), std::ref(errorstack));
    std::thread mail_thread;
    if (cfg.mail) {
        mail_thread = std::thread(mail_task, std::cref(cfg), std::ref(mail_queue), std::ref(errorstack));
    }

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
    FILE_LOG(libs::log_level::INFO) << "research started";

    {
        std::array<char, 512> buffer;
        ::gethostname(buffer.data(), buffer.size());
        libs::source_event se;
        se.control_message = true;
        se.message = "ctguard-research started";
        se.source_program = "ctguard-research";
        se.time_scanned = se.time_send = std::time(nullptr);
        se.hostname = buffer.data();
        se.source_domain = "daemon";

        input_queue.emplace(std::move(se));
    }

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

    FILE_LOG(libs::log_level::INFO) << "research shutting down...";
    RUNNING = false;

    if (cfg.mail) {
        try {
            send_mail(cfg.mail_host, cfg.mail_port, cfg.mail_fromaddr, cfg.mail_toaddr, "ctguard shutting down...", cfg.mail_replyaddr,
                      "ctguard is shutting down.");
        } catch (const std::exception & e) {
            FILE_LOG(libs::log_level::ERROR) << "Can not send shutdown mail: " << e.what();
        }
    }

    // TODO: fix shutdown time in unit tests

    FILE_LOG(libs::log_level::DEBUG) << "waiting for threads...";
    input_thread.join();
    processing_thread.join();
    state_thread.join();
    output_thread.join();
    intervention_thread.join();
    if (mail_thread.joinable()) {
        mail_thread.join();
    }
    FILE_LOG(libs::log_level::DEBUG) << "threads finished";
}

}  // namespace ctguard::research
