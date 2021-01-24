#include "daemon.hpp"

#include <sys/inotify.h>
#include <sys/select.h>
#include <unistd.h>

#include <atomic>
#include <csignal>
#include <iostream>
#include <set>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../libs/blockedqueue.hpp"
#include "../libs/errnoexception.hpp"
#include "../libs/filesystem/directory.hpp"
#include "../libs/libexception.hpp"
#include "../libs/logger.hpp"
#include "../libs/scopeguard.hpp"
#include "../libs/sqlite/sqlitestatement.hpp"
#include "../libs/sqlite/sqlitetransaction.hpp"

#include "database.hpp"
#include "eventsink.hpp"
#include "filedata.hpp"
#include "watch.hpp"

namespace ctguard::diskscan {

using errorstack_t = std::pair<std::mutex, std::stack<std::exception_ptr>>;
enum flags
{
    EMPTY = 0x0,
    REALTIME = 0x1,
    CHK_CONTENT = 0x2,
    RECURSIVE = 0x4,
    FORCE_UPDATE = 0x8,
    CHK_DIFF = 0x10,
    KILL = 0x20,
    STARTSCAN = 0x40,
    MIDSCAN = 0x80,
    ENDSCAN = 0x100,
    SPECIAL = KILL | STARTSCAN | MIDSCAN | ENDSCAN,
};

static std::atomic<bool> RUNNING{ true };
static bool UNIT_TEST;

// NOLINTNEXTLINE(fuchsia-statically-constructed-objects,google-runtime-int)
static std::atomic<long long> Talert_entire, Talert_select, Talert_update_full, Talert_update_small, Talert_delete, Talert_insert, Talert_insertq, Toutput,
  Twatch, Tevent, Trtevent, Tscan, Tevent_insert, Talert_updateq, Trtevent_insert;

static libs::source_event log_2_se(std::string source, bool control, std::string message)
{
    libs::source_event se;
    se.control_message = control;
    if (UNIT_TEST) {
        std::replace(message.begin(), message.end(), '\n', ' ');
    }
    se.message = std::move(message);
    se.source_domain = std::move(source);
    se.source_program = "ctguard-diskscan";
    if (!UNIT_TEST) {
        se.time_scanned = std::time(nullptr);
    }

    return se;
}

static void output_task(libs::blocked_queue<libs::source_event> & queue, eventsink & esink, errorstack_t & estack)
{
    FILE_LOG(libs::log_level::DEBUG) << "output-task started";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "output-task stopped"; } };

    try {
        std::queue<libs::source_event> local_queue;
        bool send_error_msg{ false };
        for (;;) {
            std::optional<libs::source_event> se{ queue.take(std::chrono::seconds(1)) };
            const auto output_start = std::chrono::high_resolution_clock::now();

            if (se.has_value()) {
                FILE_LOG(libs::log_level::DEBUG2) << "output-task input: '" << se->message << "'";

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

            const auto output_end = std::chrono::high_resolution_clock::now();
            Toutput += std::chrono::duration_cast<std::chrono::milliseconds>(output_end - output_start).count();
        }

    } catch (...) {
        std::lock_guard lg{ estack.first };
        estack.second.push(std::current_exception());
    }
}

static void add_watch(int inotify_handle, std::unordered_map<int, std::string> & watches, const std::string & path, unsigned int mask, bool recursive)
{
    const int watch = ::inotify_add_watch(inotify_handle, path.c_str(), mask);
    if (watch < 0) {
        if (errno == ENOENT) {
            FILE_LOG(libs::log_level::WARNING) << "Directory '" << path << "' can not be monitored, cause it does not exist";
        } else {
            FILE_LOG(libs::log_level::ERROR) << "Can not add inotify watch on directory '" << path << "': " << ::strerror(errno);
        }
        return;
    }
    const auto r = watches.emplace(watch, path);
    // might be added already by recursive parent directory
    if (!r.second && r.first->second != path) {
        FILE_LOG(libs::log_level::ERROR) << "Can not add inotify watch " << watch << " for path '" << path << "' to watch set, already exists with path '"
                                         << r.first->second << "'";
    }

    FILE_LOG(libs::log_level::DEBUG2) << "Adding realtime watch for directory '" << path << "' with watch id " << watch;
    FILE_LOG(libs::log_level::DEBUG3) << "ww:   in: '" << inotify_handle << "' wd: '" << watch << "' mask: '" << std::hex << mask
                                      << "' recursive: " << recursive;

    if (!recursive) {
        return;
    }

    libs::filesystem::directory dir{ path };
    for (const libs::filesystem::file_object e : dir) {
        if (!e.is_dir()) {
            continue;
        }

        if (e.is_dotordotdot()) {
            continue;
        }

        add_watch(inotify_handle, watches, path + '/' + e.name(), mask, recursive);
    }
}

static void watch_task(const diskscan_config & cfg, libs::blocked_queue<std::tuple<std::chrono::milliseconds, std::string, flags>> & queue,
                       libs::blocked_queue<std::pair<std::string, flags>> & scan_queue, const watch & watch, errorstack_t & es)
{
    FILE_LOG(libs::log_level::DEBUG) << "watch-task started";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "watch-task stopped"; } };

    try {
        const int in = ::inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
        if (in < 0) {
            throw libs::errno_exception{ "Can not init inotify" };
        }
        libs::scope_guard sg1{ [in]() { ::close(in); } };

        unsigned int mask = 0;
        switch (watch.option()) {
            case watch::scan_option::FULL:
                mask = IN_ATTRIB | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MODIFY | IN_MOVE | IN_MOVED_FROM | IN_MOVED_TO | IN_MOVE_SELF;
                break;
        }

        std::unordered_map<int, std::string> watches;
        add_watch(in, watches, watch.path(), mask, watch.recursive());

        libs::scope_guard sg2{ [&]() {
            for (const auto & i : watches) {
                if (::inotify_rm_watch(in, i.first) == -1) {
                    FILE_LOG(libs::log_level::WARNING) << "Can not cleanup delete watch " << i.first << " for '" << i.second << "': " << ::strerror(errno);
                }
            }
        } };

        constexpr size_t EVENT_SIZE = sizeof(struct inotify_event);
        constexpr size_t EVENT_BUF_LEN = 1024 * (EVENT_SIZE + 16);

        for (;;) {
            fd_set rfds;
            struct timeval tv
            {
                0, 500000
            };                  // 1/2 second
            FD_ZERO(&rfds);     // NOLINT(hicpp-no-assembler,readability-isolate-declaration)
            FD_SET(in, &rfds);  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

            const int retval = ::select(in + 1, &rfds, nullptr, nullptr, &tv);

            const auto watch_start = std::chrono::high_resolution_clock::now();

            if (retval == -1) {
                throw libs::errno_exception{ "Can not select inotify event" };
            }
            if (!retval) {
                // timeout
                FILE_LOG(libs::log_level::DEBUG2) << "wt: Watch timeout for '" << watch.path() << "'";
                if (!RUNNING) {
                    return;
                }

                const auto watch_end = std::chrono::high_resolution_clock::now();
                Twatch += std::chrono::duration_cast<std::chrono::milliseconds>(watch_end - watch_start).count();
                continue;
            }
            // data is available

            std::array<char, EVENT_BUF_LEN> buffer;  // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
            const ssize_t length = ::read(in, buffer.data(), EVENT_BUF_LEN);
            if (length < 0) {
                throw libs::errno_exception{ "Can not read inotify events" };
            }

            FILE_LOG(libs::log_level::DEBUG4) << "wt: Inotify data read: " << length << " [" << EVENT_SIZE << "]";

            for (std::size_t len = 0; len < static_cast<std::size_t>(length);) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-constant-array-index)
                const struct inotify_event * event = reinterpret_cast<struct inotify_event *>(&buffer[len]);
#pragma GCC diagnostic pop
                len += EVENT_SIZE + event->len;

                if (event->mask == IN_IGNORED) {
                    FILE_LOG(libs::log_level::DEBUG2) << "wt: Ignore event due to IN_IGNORED flag";

                    const auto watch_end = std::chrono::high_resolution_clock::now();
                    Twatch += std::chrono::duration_cast<std::chrono::milliseconds>(watch_end - watch_start).count();
                    continue;
                }

                const auto matching_watch = watches.find(event->wd);
                if (matching_watch == watches.end()) {
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
                    throw libs::lib_exception{ "No watch with desired id " + std::to_string(event->wd) + " stored, filename '" + event->name + "', mask " +
                                               std::to_string(event->mask) };
                }

                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
                const std::string full_path = event->len ? (matching_watch->second + '/' + event->name) : matching_watch->second;

                {
                    std::smatch match;
                    if (std::regex_search(full_path, match, cfg.realtime_ignore)) {
                        FILE_LOG(libs::log_level::DEBUG) << "wt: Ignore event due to realtime ignore list";

                        continue;
                    }
                }

                const auto time{ std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()) };

                switch (event->mask) {
                    case IN_DELETE_SELF:
                        FILE_LOG(libs::log_level::DEBUG1) << "wt: Directory '" << full_path << "' itself deleted";
                        queue.emplace(time, full_path, static_cast<flags>(flags::EMPTY | (watch.check_diff() ? flags::CHK_DIFF : flags::EMPTY)));

                        // no inotify_rm_watch: kernel already deleted it
                        if (watches.erase(event->wd) != 1) {
                            FILE_LOG(libs::log_level::ERROR) << "Can not erase watch " << event->wd << " from set";
                        }
                        break;

                    case IN_CREATE | IN_ISDIR:
                    case IN_MOVED_TO | IN_ISDIR:
                        queue.emplace(time, full_path, flags::CHK_CONTENT);
                        scan_queue.emplace(std::piecewise_construct, std::forward_as_tuple(full_path),
                                           std::forward_as_tuple(watch.recursive() ? flags::RECURSIVE : flags::EMPTY));
                        FILE_LOG(libs::log_level::DEBUG1) << "wt: New directory '" << full_path << "' created.";
                        add_watch(in, watches, full_path, mask, watch.recursive());
                        break;

                    case IN_CREATE:
                    case IN_MOVED_TO:
                        queue.emplace(time, full_path, static_cast<flags>(flags::CHK_CONTENT | (watch.check_diff() ? flags::CHK_DIFF : flags::EMPTY)));
                        FILE_LOG(libs::log_level::DEBUG1) << "wt: New file '" << full_path << "' created.";
                        break;

                    case IN_DELETE | IN_ISDIR:
                        queue.emplace(time, full_path, flags::CHK_CONTENT);
                        FILE_LOG(libs::log_level::DEBUG1) << "wt: Directory '" << full_path << "' deleted.";
                        break;

                    case IN_DELETE:
                    case IN_MOVED_FROM:
                        queue.emplace(time, full_path, static_cast<flags>(flags::CHK_CONTENT | (watch.check_diff() ? flags::CHK_DIFF : flags::EMPTY)));
                        FILE_LOG(libs::log_level::DEBUG1) << "wt: File '" << full_path << "' deleted.";
                        break;

                    case IN_MODIFY:
                        queue.emplace(time, full_path, static_cast<flags>(flags::CHK_CONTENT | (watch.check_diff() ? flags::CHK_DIFF : flags::EMPTY)));
                        FILE_LOG(libs::log_level::DEBUG1) << "wt: Content changed of object '" << full_path << "'.";
                        break;

                    case IN_ATTRIB:
                    case IN_ATTRIB | IN_ISDIR:
                        queue.emplace(time, full_path, flags::EMPTY);
                        FILE_LOG(libs::log_level::DEBUG1) << "wt: Attributes changed for object '" << full_path << "'.";
                        break;

                    case IN_MOVE_SELF:
                        queue.emplace(time, full_path, static_cast<flags>(flags::CHK_CONTENT | (watch.check_diff() ? flags::CHK_DIFF : flags::EMPTY)));
                        FILE_LOG(libs::log_level::DEBUG1) << "wt: Watched object was moved '" << full_path << "'.";
                        break;

                    default:
                        queue.emplace(time, full_path, flags::CHK_CONTENT);
                        FILE_LOG(libs::log_level::ERROR) << "Unknown event occurred on '" << full_path << "'; mask : '" << std::hex << event->mask << "'";
                        break;
                }
            }

            const auto watch_end = std::chrono::high_resolution_clock::now();
            Twatch += std::chrono::duration_cast<std::chrono::milliseconds>(watch_end - watch_start).count();
        }

    } catch (...) {
        std::lock_guard lg{ es.first };
        es.second.push(std::current_exception());
    }
}

static void rtevent_task(const diskscan_config & cfg, libs::blocked_queue<std::tuple<std::chrono::milliseconds, std::string, flags>> & input_queue,
                         libs::blocked_queue<std::pair<std::unique_ptr<file_data>, flags>> & output_queue, file_data_factory & fdf, errorstack_t & es)
{
    FILE_LOG(libs::log_level::DEBUG) << "rtevent-task started";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "rtevent-task stopped"; } };

    try {
        for (;;) {
            const auto & front_time_event{ input_queue.front() };
            const auto & fe_time = std::get<0>(front_time_event);
            const auto fe_flags = std::get<2>(front_time_event);

            const auto rtevent_start = std::chrono::high_resolution_clock::now();

            if ((fe_flags & flags::KILL) || !RUNNING) {
                FILE_LOG(libs::log_level::DEBUG) << "[rte] Kill event on occurred";
                output_queue.emplace(std::piecewise_construct, std::forward_as_tuple(nullptr), std::forward_as_tuple(flags::KILL));
                return;
            }

            const auto time{ std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()) };
            const auto time_diff{ fe_time + std::chrono::milliseconds(cfg.settle_time) - time };
            if (time_diff <= std::chrono::milliseconds(0)) {
                const auto & time_event{ input_queue.take() };
                const auto & e_path = std::get<1>(time_event);
                const auto e_flags = std::get<2>(time_event);
                FILE_LOG(libs::log_level::DEBUG) << "[rte] Event on '" << e_path << "' occurred: " << std::hex << e_flags;
                const auto rtevent_insert_start = std::chrono::high_resolution_clock::now();
                output_queue.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(fdf.construct(e_path, e_flags & flags::CHK_CONTENT, e_flags & flags::CHK_DIFF)),
                                     std::forward_as_tuple(e_flags));
                const auto rtevent_insert_end = std::chrono::high_resolution_clock::now();
                Trtevent_insert += std::chrono::duration_cast<std::chrono::milliseconds>(rtevent_insert_end - rtevent_insert_start).count();
                const auto rtevent_end = std::chrono::high_resolution_clock::now();
                Trtevent += std::chrono::duration_cast<std::chrono::milliseconds>(rtevent_end - rtevent_start).count();
            } else {
                const auto rtevent_end = std::chrono::high_resolution_clock::now();
                Trtevent += std::chrono::duration_cast<std::chrono::milliseconds>(rtevent_end - rtevent_start).count();
                std::this_thread::sleep_for(time_diff);
            }
        }

    } catch (...) {
        std::lock_guard lg{ es.first };
        es.second.push(std::current_exception());
    }

    output_queue.emplace(std::piecewise_construct, std::forward_as_tuple(nullptr), std::forward_as_tuple(flags::KILL));
}

static void event_task(libs::blocked_queue<std::pair<std::string, flags>> & event_queue,
                       libs::blocked_queue<std::pair<std::unique_ptr<file_data>, flags>> & file_data_queue, file_data_factory & fdf, errorstack_t & es)
{
    FILE_LOG(libs::log_level::DEBUG) << "event-task started";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "event-task stopped"; } };

    try {
        for (;;) {
            const auto & eventf{ event_queue.take() };
            const auto & path = eventf.first;
            const auto flags = eventf.second;

            const auto event_start = std::chrono::high_resolution_clock::now();

            if ((flags & flags::KILL) || !RUNNING) {
                FILE_LOG(libs::log_level::DEBUG) << "[et] Kill event on occurred";
                file_data_queue.emplace(std::piecewise_construct, std::forward_as_tuple(nullptr), std::forward_as_tuple(flags));
                return;
            }

            if (flags & flags::SPECIAL) {
                FILE_LOG(libs::log_level::DEBUG) << "[et] Special event on occurred";
                file_data_queue.emplace(std::piecewise_construct, std::forward_as_tuple(nullptr), std::forward_as_tuple(flags));
                continue;
            }

            FILE_LOG(libs::log_level::DEBUG) << "[et] Event on '" << path << "' occurred: " << std::hex << flags;

            const auto event_mid = std::chrono::high_resolution_clock::now();

            file_data_queue.emplace(std::piecewise_construct, std::forward_as_tuple(fdf.construct(path, flags & flags::CHK_CONTENT, flags & flags::CHK_DIFF)),
                                    std::forward_as_tuple(flags));

            const auto event_end = std::chrono::high_resolution_clock::now();
            Tevent_insert += std::chrono::duration_cast<std::chrono::milliseconds>(event_end - event_mid).count();
            Tevent += std::chrono::duration_cast<std::chrono::milliseconds>(event_end - event_start).count();
        }

    } catch (...) {
        std::lock_guard lg{ es.first };
        es.second.push(std::current_exception());
    }

    file_data_queue.emplace(std::piecewise_construct, std::forward_as_tuple(nullptr), std::forward_as_tuple(flags::KILL));
}

struct file_data_set_compare
{
    bool operator()(const file_data & lhs, const file_data & rhs) const { return lhs.path() < rhs.path(); }
};

static void do_insert_set(std::set<file_data, file_data_set_compare> & insert_set, database & db, libs::blocked_queue<libs::source_event> & output_queue)
{
    const auto insertq_start = std::chrono::high_resolution_clock::now();

    if (insert_set.empty()) {
        return;
    }

    libs::sqlite::sqlite_transaction tr{ db };

    libs::sqlite::sqlite_statement insert{ "INSERT INTO `diskscan-data`"
                                           " ( `name`, `user`, `group`, `size`, `inode`, `mode`, `mtime`, `ctime`, `type`, `sha256`, `target` )"
                                           " VALUES"
                                           " ( $001, $002, $003, $004, $005, $006, $007, $008, $009, $010, $011 );",
                                           db };

    for (const auto & fd : insert_set) {
        insert.bind(1, fd.path());
        insert.bind(2, fd.user());
        insert.bind(3, fd.group());
        insert.bind(4, fd.size());
        insert.bind(5, fd.inode());
        insert.bind(6, fd.mode());
        insert.bind(7, fd.mtime());
        insert.bind(8, fd.ctime());
        insert.bind(9, static_cast<unsigned int>(fd.get_type()));
        insert.bind(10, fd.content_checked() ? fd.sha256() : "not computed");
        insert.bind(11, fd.target());

        insert.run();

        insert.reset();
        insert.clear_bindings();

        std::ostringstream alert;
        alert << "ALERT: New object\n";
        alert << "Path:            " << fd.path() << '\n';
        alert << "User:            " << fd.user() << '\n';
        alert << "Group:           " << fd.group() << '\n';
        alert << "Size (in bytes): " << fd.size() << '\n';
        alert << "Mode:            " << print_mode(fd.mode()) << '\n';
        if (!UNIT_TEST) {
            alert << "Inode:           " << fd.inode() << '\n';
            alert << "Mtime:           " << print_time(fd.mtime()) << '\n';
            alert << "Ctime:           " << print_time(fd.ctime()) << '\n';
        }
        if (fd.get_type() == file_data::type::file) {
            alert << "SHA256 checksum: " << fd.sha256() << '\n';
        }
        if (fd.get_type() == file_data::type::link) {
            alert << "Target:          " << fd.target() << '\n';
            alert << "Valid:           " << std::boolalpha << (!fd.dead_lnk()) << '\n';
        }
        alert << "Type:            " << fd.get_type() << '\n';
        alert << "ALERT END";

        output_queue.emplace(log_2_se(fd.path(), false, alert.str()));
    }

    insert_set.clear();

    const auto insertq_end = std::chrono::high_resolution_clock::now();
    Talert_insertq += std::chrono::duration_cast<std::chrono::milliseconds>(insertq_end - insertq_start).count();
}

static void do_update_queue(std::queue<std::string> & update_queue, database & db)
{
    const auto updateq_start = std::chrono::high_resolution_clock::now();

    if (update_queue.empty()) {
        return;
    }

    libs::sqlite::sqlite_transaction tr{ db };

    libs::sqlite::sqlite_statement update{ "UPDATE `diskscan-data` SET"
                                           " `updated` = CURRENT_TIMESTAMP,"
                                           " `scanned` = 1"
                                           " WHERE `name` = $001;",
                                           db };

    while (!update_queue.empty()) {
        const std::string & path{ update_queue.front() };

        update.bind(1, path);
        update.run();

        update.reset();
        update.clear_bindings();

        update_queue.pop();
    }

    const auto updateq_end = std::chrono::high_resolution_clock::now();
    Talert_updateq += std::chrono::duration_cast<std::chrono::milliseconds>(updateq_end - updateq_start).count();
}

static void alert_task(const diskscan_config & cfg, libs::blocked_queue<std::pair<std::unique_ptr<file_data>, flags>> & file_data_queue,
                       libs::blocked_queue<libs::source_event> & output_queue, libs::blocked_queue<std::pair<std::string, flags>> & event_queue,
                       std::tuple<std::condition_variable, std::mutex, bool> & scan_is_running, errorstack_t & es)
{
    FILE_LOG(libs::log_level::DEBUG) << "alert-task started";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "alert-task stopped"; } };

    try {
        FILE_LOG(libs::log_level::DEBUG) << "Opening database '" + cfg.db_path + "' ...";
        database db{ cfg.db_path };

        bool in_scan{ false };
        std::set<file_data, file_data_set_compare> insert_set;
        std::queue<std::string> update_queue;

        for (;;) {
            const std::pair<std::unique_ptr<file_data>, flags> fdf{ file_data_queue.take() };
            const flags fd_flags = fdf.second;

            const auto start = std::chrono::high_resolution_clock::now();

            if ((fd_flags & flags::KILL) || !RUNNING) {
                FILE_LOG(libs::log_level::DEBUG) << "[al] Stop event occurred";

                {
                    libs::source_event se;
                    se.control_message = true;
                    se.message = "!KILL";
                    output_queue.emplace(std::move(se));
                }

                const auto end = std::chrono::high_resolution_clock::now();
                Talert_entire += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                return;
            }

            if (fd_flags & flags::STARTSCAN) {
                FILE_LOG(libs::log_level::DEBUG) << "[al] Start scan event occurred";

                libs::sqlite::sqlite_statement set_scanned_false{ "UPDATE `diskscan-data` SET `scanned` = 0", db };
                set_scanned_false.run();
                in_scan = true;

                const auto end = std::chrono::high_resolution_clock::now();
                Talert_entire += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                continue;
            }

            if (fd_flags & flags::MIDSCAN) {
                FILE_LOG(libs::log_level::DEBUG) << "[al] Scan reached phase 2...";

                libs::sqlite::sqlite_statement select_non_scanned{ "SELECT `name` FROM `diskscan-data` WHERE `scanned` = 0", db };
                const auto & ret = select_non_scanned.run(true);

                FILE_LOG(libs::log_level::INFO) << "Running phase 2 for " << ret.data_count() << " entries...";

                for (const auto column : ret) {
                    const char * db_name = column.get<const char *>(0);
                    // TODO(cgzones): check_diff
                    event_queue.emplace(db_name, static_cast<flags>(flags::CHK_CONTENT | flags::FORCE_UPDATE));
                }

                event_queue.emplace("", flags::ENDSCAN);

                in_scan = false;
                do_insert_set(insert_set, db, output_queue);
                do_update_queue(update_queue, db);

                const auto end = std::chrono::high_resolution_clock::now();
                Talert_entire += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                continue;
            }

            if (fd_flags & flags::ENDSCAN) {
                FILE_LOG(libs::log_level::DEBUG) << "[al] End scan event occurred";

                libs::sqlite::sqlite_statement select_non_scanned{ "SELECT `name` FROM `diskscan-data` WHERE `scanned` = 0", db };
                const auto & ret = select_non_scanned.run(true);
                if (ret.data_count() != 0) {
                    FILE_LOG(libs::log_level::WARNING) << "The following database entries could not be scanned:";
                    for (const auto column : ret) {
                        const char * db_name = column.get<const char *>(0);
                        FILE_LOG(libs::log_level::WARNING) << "Database entry was not scanned: '" << db_name << "'";
                    }
                }

                {
                    std::lock_guard lg{ std::get<std::mutex>(scan_is_running) };
                    std::get<bool>(scan_is_running) = false;
                    std::get<std::condition_variable>(scan_is_running).notify_all();
                }

                FILE_LOG(libs::log_level::INFO) << "Scan finished.";

                const auto end = std::chrono::high_resolution_clock::now();
                Talert_entire += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                continue;
            }

            if (!fdf.first) {
                FILE_LOG(libs::log_level::ERROR) << "NULL file_data passed to alert_task().";
                continue;
            }
            const file_data & fd = *fdf.first;

            FILE_LOG(libs::log_level::DEBUG) << "[al] Event on '" << fd.path() << "' occurred";

            const auto select_start = std::chrono::high_resolution_clock::now();
            libs::sqlite::sqlite_statement select{ "SELECT * FROM `diskscan-data` WHERE `name` = $001", db };

            select.bind(1, fd.path());
            const auto & ret = select.run(true);
            bool found{ false };

            for (const auto column : ret) {
                if (found) {
                    FILE_LOG(libs::log_level::ERROR) << "Multiple db entries for '" << fd.path() << '\'';
                }
                found = true;

                // const char * db_name = column.get<const char *>(0);
                const auto & db_user = column.get<const char *>(1);
                const auto & db_group = column.get<const char *>(2);
                const auto & db_size = column.get<file_data::size_type>(3);
                const auto & db_inode = column.get<file_data::inode_type>(4);
                const auto & db_mode = column.get<file_data::mode_type>(5);
                const auto & db_mtime = column.get<file_data::mtime_type>(6);
                const auto & db_ctime = column.get<file_data::ctime_type>(7);
                const auto & db_type = column.get<unsigned int>(8);
                const auto & db_sha256 = column.get<const char *>(9);
                const auto & db_target = column.get<const char *>(10);

                if (fd.exists()) {
                    bool changed{ false };
                    std::ostringstream alert;
                    alert << "ALERT: object changed\n";
                    alert << "Path:            " << fd.path() << '\n';
                    alert << "Type:            " << fd.get_type() << '\n';

                    if (db_user != fd.user()) {
                        changed = true;

                        alert << "User changed from '" << db_user << "' to '" << fd.user() << "'\n";
                    }
                    if (db_group != fd.group()) {
                        changed = true;

                        alert << "Group changed from '" << db_group << "' to '" << fd.group() << "'\n";
                    }
                    if (db_size != fd.size()) {
                        changed = true;

                        alert << "Size changed from '" << db_size << "' to '" << fd.size() << "' bytes\n";
                    }
                    if (db_mode != fd.mode()) {
                        changed = true;

                        alert << "Mode changed from '" << print_mode(db_mode) << "' to '" << print_mode(fd.mode()) << "'\n";
                    }
                    if (db_inode != fd.inode() && !UNIT_TEST) {
                        // changed = true; do not create an alert for this event only

                        alert << "Inode changed from '" << db_inode << "' to '" << fd.inode() << "'\n";
                    }
                    if (db_mtime != fd.mtime() && !UNIT_TEST) {
                        // changed = true; do not create an alert for this event only

                        alert << "Mtime changed from '" << print_time(db_mtime) << "' to '" << print_time(fd.mtime()) << "'\n";
                    }
                    if (db_ctime != fd.ctime() && !UNIT_TEST) {
                        // changed = true; do not create an alert for this event only

                        alert << "Ctime changed from '" << print_time(db_ctime) << "' to '" << print_time(fd.ctime()) << "'\n";
                    }
                    if (fd.get_type() == file_data::type::file && fd.content_checked() && fd.sha256() != db_sha256) {
                        changed = true;

                        alert << "SHA256 checksum changed from '" << db_sha256 << "' to '" << fd.sha256() << "'\n";
                    }
                    if (fd.get_type() == file_data::type::link) {
                        if (fd.content_checked() && db_target != fd.target()) {
                            changed = true;
                            alert << "Target changed from '" << db_target << "' to '" << fd.target() << "'\n";
                        }
                    }
                    if (db_type != static_cast<unsigned int>(fd.get_type())) {
                        changed = true;

                        alert << "Type changed from '" << static_cast<file_data::type>(db_type) << "' to '" << fd.get_type() << "'\n";
                    }

                    if (!fd.diff().empty()) {
                        alert << "Start Diff\n" << fd.diff() << "End Diff\n";
                    }

                    alert << "ALERT END";

                    if (changed) {
                        const auto update_full_start = std::chrono::high_resolution_clock::now();
                        libs::sqlite::sqlite_statement update{ "UPDATE `diskscan-data` SET"
                                                               " `user` = $001,"
                                                               " `group` = $002,"
                                                               " `size` = $003,"
                                                               " `inode` = $004,"
                                                               " `mode` = $005,"
                                                               " `mtime` = $006,"
                                                               " `ctime` = $007,"
                                                               " `sha256` = $008,"
                                                               " `type` = $009,"
                                                               " `target` = $010,"
                                                               " `updated` = CURRENT_TIMESTAMP,"
                                                               " `scanned` = 1"
                                                               " WHERE `name` = $011;",
                                                               db };

                        update.bind(1, fd.user());
                        update.bind(2, fd.group());
                        update.bind(3, fd.size());
                        update.bind(4, fd.inode());
                        update.bind(5, fd.mode());
                        update.bind(6, fd.mtime());
                        update.bind(7, fd.ctime());
                        update.bind(8, fd.content_checked() ? fd.sha256() : db_sha256);
                        update.bind(9, static_cast<unsigned int>(fd.get_type()));
                        update.bind(10, fd.target());
                        update.bind(11, fd.path());
                        update.run();

                        output_queue.emplace(log_2_se(fd.path(), false, alert.str()));

                        const auto update_full_end = std::chrono::high_resolution_clock::now();
                        Talert_update_full += std::chrono::duration_cast<std::chrono::milliseconds>(update_full_end - update_full_start).count();
                    } else if (fd_flags & flags::FORCE_UPDATE) {
                        if (!in_scan) {
                            const auto update_small_start = std::chrono::high_resolution_clock::now();
                            libs::sqlite::sqlite_statement update{ "UPDATE `diskscan-data` SET"
                                                                   " `updated` = CURRENT_TIMESTAMP,"
                                                                   " `scanned` = 1"
                                                                   " WHERE `name` = $001;",
                                                                   db };
                            update.bind(1, fd.path());
                            update.run();
                            const auto update_small_end = std::chrono::high_resolution_clock::now();
                            Talert_update_small += std::chrono::duration_cast<std::chrono::milliseconds>(update_small_end - update_small_start).count();
                        } else {
                            update_queue.emplace(fd.path());

                            if (update_queue.size() >= cfg.block_size) {
                                do_update_queue(update_queue, db);
                            }
                        }
                    }

                } else { /* not fd.exists() */
                    const auto delete_start = std::chrono::high_resolution_clock::now();
                    libs::sqlite::sqlite_statement remove{ "DELETE FROM `diskscan-data` "
                                                           " WHERE `name` = $001;",
                                                           db };
                    remove.bind(1, fd.path());
                    remove.run();

                    const auto delete_end = std::chrono::high_resolution_clock::now();
                    Talert_delete += std::chrono::duration_cast<std::chrono::milliseconds>(delete_end - delete_start).count();

                    std::ostringstream alert;
                    alert << "ALERT: object deleted\n";
                    alert << "Path:            " << fd.path() << '\n';
                    alert << "Type:            " << static_cast<file_data::type>(db_type) << '\n';
                    alert << "ALERT END";

                    output_queue.emplace(log_2_se(fd.path(), false, alert.str()));
                }
            }

            const auto select_end = std::chrono::high_resolution_clock::now();
            Talert_select += std::chrono::duration_cast<std::chrono::milliseconds>(select_end - select_start).count();

            if (!found && fd.exists()) {
                const auto insert_start = std::chrono::high_resolution_clock::now();
                if (!in_scan) {
                    libs::sqlite::sqlite_statement insert{ "INSERT INTO `diskscan-data`"
                                                           " ( `name`, `user`, `group`, `size`, `inode`, `mode`, `mtime`, `ctime`, `type`, `sha256`, `target` )"
                                                           " VALUES"
                                                           " ( $001, $002, $003, $004, $005, $006, $007, $008, $009, $010, $011 );",
                                                           db };

                    insert.bind(1, fd.path());
                    insert.bind(2, fd.user());
                    insert.bind(3, fd.group());
                    insert.bind(4, fd.size());
                    insert.bind(5, fd.inode());
                    insert.bind(6, fd.mode());
                    insert.bind(7, fd.mtime());
                    insert.bind(8, fd.ctime());
                    insert.bind(9, static_cast<unsigned int>(fd.get_type()));
                    insert.bind(10, fd.content_checked() ? fd.sha256() : "not computed");
                    insert.bind(11, fd.target());

                    insert.run();

                    std::ostringstream alert;
                    alert << "ALERT: New object\n";
                    alert << "Path:            " << fd.path() << '\n';
                    alert << "User:            " << fd.user() << '\n';
                    alert << "Group:           " << fd.group() << '\n';
                    alert << "Size (in bytes): " << fd.size() << '\n';
                    alert << "Mode:            " << print_mode(fd.mode()) << '\n';
                    if (!UNIT_TEST) {
                        alert << "Inode:           " << fd.inode() << '\n';
                        alert << "Mtime:           " << print_time(fd.mtime()) << '\n';
                        alert << "Ctime:           " << print_time(fd.ctime()) << '\n';
                    }
                    if (fd.get_type() == file_data::type::file) {
                        alert << "SHA256 checksum: " << fd.sha256() << '\n';
                    }
                    if (fd.get_type() == file_data::type::link) {
                        alert << "Target:          " << fd.target() << '\n';
                        alert << "Valid:           " << std::boolalpha << (!fd.dead_lnk()) << '\n';
                    }
                    alert << "Type:            " << fd.get_type() << '\n';
                    alert << "ALERT END";

                    output_queue.emplace(log_2_se(fd.path(), false, alert.str()));

                    const auto insert_end = std::chrono::high_resolution_clock::now();
                    Talert_insert += std::chrono::duration_cast<std::chrono::milliseconds>(insert_end - insert_start).count();
                } else {
                    const auto ins = insert_set.insert(fd);
                    if (!ins.second) {
                        insert_set.erase(ins.first);
                        const auto ins2 = insert_set.insert(fd);
                        if (!ins2.second) {
                            FILE_LOG(libs::log_level::ERROR) << "Can not insert '" << fd.path() << "' into insert set";
                        }
                    }

                    if (insert_set.size() >= cfg.block_size) {
                        do_insert_set(insert_set, db, output_queue);
                    }
                }
            }

            const auto end = std::chrono::high_resolution_clock::now();
            Talert_entire += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        }

    } catch (...) {
        std::lock_guard lg{ es.first };
        es.second.push(std::current_exception());
    }
    {
        libs::source_event se;
        se.control_message = true;
        se.message = "!KILL";
        output_queue.emplace(std::move(se));
    }
}

static void scan(const std::string & path, bool recursive, bool check_diff, libs::blocked_queue<std::pair<std::string, flags>> & event_queue)
{
    try {
        libs::filesystem::directory dir{ path };

        for (const libs::filesystem::file_object e : dir) {
            if (::strcmp(e.name(), ".") == 0 || ::strcmp(e.name(), "..") == 0) {
                continue;
            }

            std::string child_path{ path + '/' + e.name() };

            event_queue.emplace(child_path, static_cast<flags>(flags::CHK_CONTENT | flags::FORCE_UPDATE | (check_diff ? flags::CHK_DIFF : flags::EMPTY)));

            if (recursive && e.is_dir()) {
                scan(child_path, recursive, check_diff, event_queue);
            }
        }
    } catch (const std::exception & e) {
        FILE_LOG(libs::log_level::ERROR) << "Error scanning directory " << path << ": " << e.what();
    }
}

static void scan_task(libs::blocked_queue<std::pair<std::string, flags>> & scan_queue, libs::blocked_queue<std::pair<std::string, flags>> & event_queue,
                      errorstack_t & es)
{
    FILE_LOG(libs::log_level::DEBUG) << "scan-task started";
    libs::scope_guard sg{ []() { FILE_LOG(libs::log_level::DEBUG) << "scan-task stopped"; } };

    try {
        for (;;) {
            const auto input{ scan_queue.take() };

            const auto scan_start = std::chrono::high_resolution_clock::now();

            const auto & path = input.first;
            const auto flags = input.second;

            if ((flags & flags::KILL) || !RUNNING) {
                FILE_LOG(libs::log_level::DEBUG) << "scan-task kill";
                event_queue.emplace("", flags::KILL);
                return;
            }

            if (path.empty()) {
                if (flags & flags::SPECIAL) {
                    event_queue.emplace("", flags);
                } else {
                    FILE_LOG(libs::log_level::ERROR) << "Empty scan path";
                }
            } else {
                scan(path, flags & flags::RECURSIVE, flags & flags::CHK_DIFF, event_queue);
            }

            const auto scan_end = std::chrono::high_resolution_clock::now();
            Tscan += std::chrono::duration_cast<std::chrono::milliseconds>(scan_end - scan_start).count();
        }

    } catch (...) {
        std::lock_guard lg{ es.first };
        es.second.push(std::current_exception());
    }

    event_queue.emplace("", flags::KILL);
}

void daemon(const diskscan_config & cfg, bool singlerun, bool unit_test)
{
    UNIT_TEST = unit_test;
    libs::scope_guard endmsg{ []() { FILE_LOG(libs::log_level::INFO) << "ctguard-diskscan stopped"; } };

    libs::blocked_queue<std::tuple<std::chrono::milliseconds, std::string, flags>> rtevent_queue;
    libs::blocked_queue<std::pair<std::string, flags>> event_queue;
    libs::blocked_queue<std::pair<std::unique_ptr<file_data>, flags>> file_data_queue;
    libs::blocked_queue<libs::source_event> output_queue;
    libs::blocked_queue<std::pair<std::string, flags>> scan_queue;
    file_data_factory fd_factory{ cfg.diffdb_path, cfg.max_diff_size };

    errorstack_t es;

    eventsink esink{ cfg.output_path, cfg.output_kind, UNIT_TEST };

    std::tuple<std::condition_variable, std::mutex, bool> scan_is_running;
    std::get<bool>(scan_is_running) = false;

    FILE_LOG(libs::log_level::DEBUG) << "starting threads...";

    std::vector<std::thread> watch_workers;

    auto scan_thread = std::thread(scan_task, std::ref(scan_queue), std::ref(event_queue), std::ref(es));
    auto event_thread = std::thread(event_task, std::ref(event_queue), std::ref(file_data_queue), std::ref(fd_factory), std::ref(es));
    auto alert_thread = std::thread(alert_task, std::cref(cfg), std::ref(file_data_queue), std::ref(output_queue), std::ref(event_queue),
                                    std::ref(scan_is_running), std::ref(es));
    auto output_thread = std::thread(output_task, std::ref(output_queue), std::ref(esink), std::ref(es));

    FILE_LOG(libs::log_level::DEBUG) << "threads started";

    FILE_LOG(libs::log_level::INFO) << "Starting initial scan...";
    {
        std::lock_guard<std::mutex> lg{ std::get<std::mutex>(scan_is_running) };
        std::get<bool>(scan_is_running) = true;
    }
    scan_queue.emplace("", flags::STARTSCAN);
    for (const watch & w : cfg.watches) {
        scan_queue.emplace(w.path(), static_cast<flags>((w.recursive() ? flags::RECURSIVE : flags::EMPTY) | (w.check_diff() ? flags::CHK_DIFF : flags::EMPTY)));
    }
    scan_queue.emplace("", flags::MIDSCAN);
    FILE_LOG(libs::log_level::INFO) << "Initial scan initialized.";

    if (singlerun) {
        FILE_LOG(libs::log_level::INFO) << "Waiting for scan finished...";
        {
            std::unique_lock<std::mutex> ul{ std::get<std::mutex>(scan_is_running) };
            while (std::get<bool>(scan_is_running)) {
                std::get<std::condition_variable>(scan_is_running).wait(ul);
            }
        }

        FILE_LOG(libs::log_level::INFO) << "ctguard-diskscan shutting down after singlerun...";

        FILE_LOG(libs::log_level::DEBUG) << "Requesting worker(s) to stop...";
        scan_queue.emplace("", flags::KILL);
        // no RUNNING = false;

        FILE_LOG(libs::log_level::DEBUG) << "Requested worker(s) to stop; waitng for them...";

        scan_thread.join();
        event_thread.join();
        alert_thread.join();
        output_thread.join();

        return;
    }

    FILE_LOG(libs::log_level::INFO) << "Starting real time monitoring...";

    auto rtevent_thread = std::thread(rtevent_task, std::cref(cfg), std::ref(rtevent_queue), std::ref(file_data_queue), std::ref(fd_factory), std::ref(es));

    watch_workers.reserve(cfg.watches.size());

    for (const watch & w : cfg.watches) {
        if (w.realtime()) {
            watch_workers.emplace_back(watch_task, std::cref(cfg), std::ref(rtevent_queue), std::ref(scan_queue), std::cref(w), std::ref(es));
        }
    }
    FILE_LOG(libs::log_level::INFO) << "Realtime monitoring started.";

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
    FILE_LOG(libs::log_level::DEBUG) << "Waiting for signal...";

    uint64_t scantimer = 0;

    FILE_LOG(libs::log_level::INFO) << "ctguard-diskscan started";

    for (;;) {
        if (sigtimedwait(&signal_set, &info, &timeout) < 0) {
            if (errno == EINTR) {
                /* other signal occurred */
                FILE_LOG(libs::log_level::WARNING) << "Unregistered signal occurred";
                continue;
            }
            if (errno == EAGAIN) {
                /* Timeout, checking threads */
                FILE_LOG(libs::log_level::DEBUG2) << "Timeout";
                bool exc = false;
                std::lock_guard<std::mutex> lg{ es.first };
                while (!es.second.empty()) {
                    exc = true;  // NOLINT(clang-analyzer-deadcode.DeadStores)
                    try {
                        std::exception_ptr exp{ es.second.top() };
                        es.second.pop();
                        std::rethrow_exception(exp);
                    } catch (const std::exception & e) {
                        FILE_LOG(libs::log_level::ERROR) << "Thread Exception: " << e.what();
                    }
                }
                if (exc) {
                    FILE_LOG(libs::log_level::ERROR) << "Exiting due to thread exception!";
                    break;
                }
                scantimer += 1; /* timeout value */

                if (scantimer % 60 == 0) {
                    std::ostringstream oss;

                    if (Talert_entire) {
                        oss << "|alert_entire: " << Talert_entire;
                    }
                    if (Talert_select) {
                        oss << "|alert_select: " << Talert_select;
                    }
                    if (Talert_update_full) {
                        oss << "|alert_update_full: " << Talert_update_full;
                    }
                    if (Talert_update_small) {
                        oss << "|alert_update_small: " << Talert_update_small;
                    }
                    if (Talert_updateq) {
                        oss << "|alert_updateq: " << Talert_updateq;
                    }
                    if (Talert_delete) {
                        oss << "|alert_delete: " << Talert_delete;
                    }
                    if (Talert_insert) {
                        oss << "|alert_insert: " << Talert_insert;
                    }
                    if (Talert_insertq) {
                        oss << "|alert_insertq: " << Talert_insertq;
                    }
                    if (Toutput) {
                        oss << "|output: " << Toutput;
                    }
                    if (Twatch) {
                        oss << "|watch: " << Twatch;
                    }
                    if (Tevent) {
                        oss << "|event: " << Tevent;
                    }
                    if (Tevent_insert) {
                        oss << "|event_insert: " << Tevent_insert;
                    }
                    if (Trtevent) {
                        oss << "|rtevent: " << Trtevent;
                    }
                    if (Trtevent_insert) {
                        oss << "|rtevent_insert: " << Trtevent_insert;
                    }
                    if (Tscan) {
                        oss << "|scan: " << Tscan;
                    }
                    if (!rtevent_queue.empty()) {
                        oss << "|#rteventq: " << rtevent_queue.size();
                    }
                    if (!event_queue.empty()) {
                        oss << "|#eventq: " << event_queue.size();
                    }
                    if (!file_data_queue.empty()) {
                        oss << "|#filedataq: " << file_data_queue.size();
                    }
                    if (!output_queue.empty()) {
                        oss << "|#outputq: " << output_queue.size();
                    }
                    if (!scan_queue.empty()) {
                        oss << "|#scanq: " << scan_queue.size();
                    }
                    if (oss.str().empty()) {
                        FILE_LOG(libs::log_level::INFO) << "Perf" << oss.str();
                    }

                    Talert_delete = Talert_entire = Talert_insert = Talert_insertq = Talert_select = Talert_update_full = Talert_update_small = Tevent =
                      Tevent_insert = Toutput = Trtevent = Tscan = Twatch = Talert_updateq = Trtevent_insert = 0;
                }

                if (scantimer >= cfg.scaninterval) {
                    FILE_LOG(libs::log_level::INFO) << "Starting regular scan...";
                    {
                        std::lock_guard<std::mutex> lg2{ std::get<std::mutex>(scan_is_running) };
                        std::get<bool>(scan_is_running) = true;
                    }
                    scan_queue.emplace("", flags::STARTSCAN);
                    for (const auto & w : cfg.watches) {
                        scan_queue.emplace(
                          w.path(), static_cast<flags>((w.recursive() ? flags::RECURSIVE : flags::EMPTY) | (w.check_diff() ? flags::CHK_DIFF : flags::EMPTY)));
                    }
                    scan_queue.emplace("", flags::MIDSCAN);
                    FILE_LOG(libs::log_level::INFO) << "Regular scan initialized.";
                    scantimer = 0;
                }
                continue;
            }

            throw libs::errno_exception{ "Error during sigtimedwait()" };
        }

        /* requested signal occurred */
        FILE_LOG(libs::log_level::DEBUG) << "Registered signal found.";
        break;
    }

    FILE_LOG(libs::log_level::INFO) << "ctguard-diskscan shutting down...";

    {
        std::lock_guard<std::mutex> lg{ std::get<std::mutex>(scan_is_running) };
        if (std::get<bool>(scan_is_running)) {
            FILE_LOG(libs::log_level::WARNING) << "Aborting current running scan.";
        }
    }

    RUNNING = false;
    scan_queue.emplace("", flags::KILL);
    rtevent_queue.emplace(std::chrono::milliseconds(0), "", flags::KILL);

    FILE_LOG(libs::log_level::DEBUG) << "waiting for threads...";
    scan_thread.join();
    event_thread.join();
    rtevent_thread.join();
    alert_thread.join();
    output_thread.join();
    for (auto & w : watch_workers) {
        w.join();
    }
    FILE_LOG(libs::log_level::DEBUG) << "threads finished";
}

} /* namespace ctguard::diskscan */
