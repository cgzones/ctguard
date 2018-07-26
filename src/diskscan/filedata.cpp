#include "filedata.hpp"

#include <sstream>

#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../libs/errnoexception.hpp"
#include "../libs/logger.hpp"
#include "../libs/scopeguard.hpp"
#include <dtl/dtl.hpp>

#include "../libs/sqlite/sqlitestatement.hpp"

namespace ctguard::diskscan {

static std::vector<std::string> split(const std::string & str, char delim = '\n')
{
    std::vector<std::string> cont;
    std::size_t current, previous = 0;
    current = str.find(delim);
    while (current != std::string::npos) {
        cont.push_back(str.substr(previous, current - previous));
        previous = current + 1;
        current = str.find(delim, previous);
    }
    cont.push_back(str.substr(previous, current - previous));
    return cont;
}

file_data file_data_factory::construct(std::string path, bool check_content, bool check_diff)
{
    if (path == "") {
        return {};
    }

    file_data fd;

    fd.m_path = std::move(path);

    struct stat s;
    if (lstat(fd.m_path.c_str(), &s) == -1) {
        if (errno == ENOENT) {
            FILE_LOG(libs::log_level::DEBUG) << "Object seems to be deleted: '" << fd.m_path << "'";
        } else {
            FILE_LOG(libs::log_level::ERROR) << "Can not stat object '" << fd.m_path << "': " << ::strerror(errno);
        }

        fd.m_exists = false;
        fd.m_content_checked = true;
        fd.m_user = "null";
        fd.m_group = "null";
        fd.m_size = 0;
        fd.m_mode = 0;
        fd.m_inode = 0;
        fd.m_mtime = 0;
        fd.m_ctime = 0;
        fd.m_type = file_data::type::unknown;
        fd.m_sha256[0] = '\0';
        fd.m_target = "";
        fd.m_dead_lnk = false;

        return fd;
    }

    FILE_LOG(libs::log_level::DEBUG) << "Computing file_data for '" << fd.m_path << "': " << std::boolalpha << check_content << " " << check_diff;

    fd.m_exists = true;

    {
        const struct passwd * user_tmp = ::getpwuid(s.st_uid);
        if (user_tmp == nullptr) {
            FILE_LOG(libs::log_level::ERROR) << "Can not get user information for uid '" << s.st_uid << "': " << ::strerror(errno);
            fd.m_user = "!unknown";
        } else {
            fd.m_user = user_tmp->pw_name;
        }
    }

    {
        const struct group * group_tmp = ::getgrgid(s.st_gid);
        if (group_tmp == nullptr) {
            FILE_LOG(libs::log_level::ERROR) << "Can not get group information for gid '" << s.st_gid << "': " << ::strerror(errno);
            fd.m_group = "!unknown";
        } else {
            fd.m_group = group_tmp->gr_name;
        }
    }

    fd.m_size = s.st_size;
    fd.m_mode = s.st_mode;
    fd.m_inode = s.st_ino;
    fd.m_mtime = s.st_mtime;
    fd.m_ctime = s.st_ctime;

    switch (s.st_mode & S_IFMT) {
        case S_IFBLK:
            fd.m_type = file_data::type::block_device;
            break;
        case S_IFCHR:
            fd.m_type = file_data::type::character_device;
            break;
        case S_IFDIR:
            fd.m_type = file_data::type::directory;
            break;
        case S_IFIFO:
            fd.m_type = file_data::type::fifo;
            break;
        case S_IFLNK:
            fd.m_type = file_data::type::link;
            break;
        case S_IFREG:
            fd.m_type = file_data::type::file;
            break;
        case S_IFSOCK:
            fd.m_type = file_data::type::socket;
            break;
        default:
            fd.m_type = file_data::type::unknown;
            FILE_LOG(libs::log_level::ERROR) << "Unknown file type for '" << fd.m_path << "': " << std::hex << s.st_mode;
            break;
    }

    if (check_content && fd.m_type == file_data::type::file) {
        bool compute_diff = check_diff && fd.m_size < m_max_diff_size;
        std::string db_content;
        std::stringstream ocontent;
        bool found_content = false;

        std::array<unsigned char, 16384> buf;

        const int file = ::open(fd.m_path.c_str(), O_RDONLY);
        if (file == -1) {
            if (errno == EACCES) {
                FILE_LOG(libs::log_level::WARNING) << "Can not open file '" << fd.m_path << "': " << ::strerror(errno);
            } else {
                FILE_LOG(libs::log_level::ERROR) << "Can not open file '" << fd.m_path << "': " << ::strerror(errno);
            }
            return fd;
        }

        libs::scope_guard sg{ [file]() { ::close(file); } };

        if (compute_diff) {
            libs::sqlite::sqlite_statement select{ "SELECT `content` FROM `diskscan-diffdata` WHERE `name` = $001;", m_db };
            select.bind(1, fd.m_path);
            const auto & ret = select.run(true);

            for (const auto column : ret) {
                if (found_content) {
                    FILE_LOG(libs::log_level::ERROR) << "Multiple diffdb entries for '" << fd.path() << '\'';
                }
                found_content = true;

                db_content = column.get<std::string_view>(0);
            }
        }

        ssize_t ret;
        SHA256_CTX ctx;
        SHA256_Init(&ctx);

        while ((ret = ::read(file, buf.data(), buf.size())) > 0) {
            SHA256_Update(&ctx, buf.data(), static_cast<size_t>(ret));
            if (compute_diff) {
                if (memchr(buf.data(), '\0', static_cast<size_t>(ret)) != nullptr) {
                    FILE_LOG(libs::log_level::DEBUG) << "File '" << fd.m_path << "' has binary format";
                    compute_diff = false;
                } else {
                    ocontent << std::string_view{ reinterpret_cast<const char *>(buf.data()), static_cast<size_t>(ret) };
                    // oss.write(&buffer.data()[0], n);
                }
            }
        }

        if (ret == -1) {
            FILE_LOG(libs::log_level::ERROR) << "Can not read from file '" << fd.m_path << "': " << ::strerror(errno);
            return fd;
        }

        SHA256_End(&ctx, fd.m_sha256.data());

        if (compute_diff) {
            const std::string content{ ocontent.str() };
            if (found_content) {
                libs::sqlite::sqlite_statement update{ "UPDATE `diskscan-diffdata` SET `content` = $001 WHERE `name` = $002;", m_db };
                update.bind(1, content);
                update.bind(2, fd.m_path);
                update.run();
            } else {
                libs::sqlite::sqlite_statement insert{ "INSERT INTO `diskscan-diffdata` ( `name`, `content` ) VALUES ( $001, $002 );", m_db };
                insert.bind(1, fd.m_path);
                insert.bind(2, content);
                insert.run();
            }

            if (db_content.empty()) {
                FILE_LOG(libs::log_level::DEBUG) << "File '" << fd.m_path << "' has no stored content";
            } else {
                dtl::Diff<std::string> diff{ split(db_content), split(content) };
                diff.onHuge();
                diff.compose();
                diff.composeUnifiedHunks();

                std::ostringstream oss;
                diff.printUnifiedFormat(oss);
                fd.m_diff = oss.str();
            }
        }

        fd.m_content_checked = true;
    }

    else if (check_content && fd.m_type == file_data::type::link) {
        struct stat tmp;
        if (::stat(fd.m_path.c_str(), &tmp) == -1) {
            if (errno == ENOENT) {
                FILE_LOG(libs::log_level::DEBUG) << "stat(" << fd.m_path << ") failed, cause link destination is dead";
            } else {
                FILE_LOG(libs::log_level::ERROR) << "Can not stat link target of '" << fd.m_path << "': " << ::strerror(errno);
            }

            fd.m_dead_lnk = true;
        } else {
            FILE_LOG(libs::log_level::DEBUG2) << "stat(" << fd.m_path << ") succeeded";
            fd.m_dead_lnk = false;
        }

        std::array<char, 256> buf;
        ssize_t ret;
        if ((ret = ::readlink(fd.m_path.c_str(), buf.data(), buf.size())) < 0) {
            FILE_LOG(libs::log_level::ERROR) << "Can not readlink for '" << fd.m_path << "': " << ::strerror(errno);
            return fd;
        }
        if (static_cast<size_t>(ret) + 1 >= buf.size()) {
            FILE_LOG(libs::log_level::WARNING) << "Target of link '" << fd.m_path << "' is longer than " << buf.size()
                                               << " characters; will countinue truncated";
            buf[buf.size() - 1] = '\0';
        } else {
            buf[static_cast<size_t>(ret)] = '\0';
        }

        fd.m_target = buf.data();

        fd.m_content_checked = true;
    }

    return fd;
}

std::ostream & operator<<(std::ostream & out, const file_data & fd)
{
    out << "Start file data\n";
    if (fd.m_exists) {
        out << "path:    " << fd.path() << '\n';
        out << "user:    " << fd.user() << '\n';
        out << "group:   " << fd.group() << '\n';
        out << "size:    " << fd.size() << '\n';
        out << "inode:   " << fd.inode() << '\n';
        out << "mode:    " << print_mode(fd.mode()) << '\n';
        out << "mtime    " << fd.mtime() << '\n';
        out << "ctime    " << fd.ctime() << '\n';
        out << "type:    " << fd.get_type() << '\n';
        out << "checked: " << fd.content_checked() << '\n';
        if (fd.get_type() == file_data::type::file) {
            out << "sha256:  " << fd.sha256() << '\n';
        } else if (fd.get_type() == file_data::type::link) {
            out << "valid:   " << std::boolalpha << !fd.dead_lnk() << '\n';
            if (!fd.dead_lnk()) {
                out << "target:  " << fd.target() << '\n';
            }
        }
    } else {
        out << "object deleted.\n";
    }
    out << "End file data\n";

    return out;
}

std::ostream & operator<<(std::ostream & out, const file_data::type t)
{
    switch (t) {
        case file_data::type::block_device:
            out << "block device";
            break;
        case file_data::type::character_device:
            out << "character device";
            break;
        case file_data::type::socket:
            out << "socket";
            break;
        case file_data::type::directory:
            out << "directory";
            break;
        case file_data::type::fifo:
            out << "fifo/pipe";
            break;
        case file_data::type::file:
            out << "regular file";
            break;
        case file_data::type::link:
            out << "symbolic link";
            break;
        case file_data::type::unknown:
            out << "unknown object type";
            break;
    }

    return out;
}

static char mode_to_type(file_data::mode_type m)
{
    if (S_ISREG(m))
        return '-';
    else if (S_ISDIR(m))
        return 'd';
    else if (S_ISBLK(m))
        return 'b';
    else if (S_ISCHR(m))
        return 'c';
    else if (S_ISFIFO(m))
        return 'p';
    else if (S_ISLNK(m))
        return 'l';
    else if (S_ISSOCK(m))
        return 's';
    else
        return '?';
}

std::string print_mode(file_data::mode_type m)
{
    std::ostringstream oss;

    oss << mode_to_type(m) << ((m & S_IRUSR) ? 'r' : '-') << ((m & S_IWUSR) ? 'w' : '-')
        << ((m & S_ISUID) ? (m & S_IXUSR) ? 's' : 'S' : ((m & S_IXUSR) ? 'x' : '-')) << ((m & S_IRGRP) ? 'r' : '-') << ((m & S_IWGRP) ? 'w' : '-')
        << ((m & S_ISGID) ? ((m & S_IXGRP) ? 's' : 'l') : ((m & S_IXGRP) ? 'x' : '-')) << ((m & S_IROTH) ? 'r' : '-') << ((m & S_IWOTH) ? 'w' : '-')
        << ((m & S_ISVTX) ? ((m & S_IXOTH) ? 't' : 'T') : ((m & S_IXOTH) ? 'x' : '-'));

    return oss.str();
}

std::string print_time(time_t t)
{
    struct tm ts;
    ::localtime_r(&t, &ts);

    std::ostringstream oss;
    oss << std::put_time(&ts, "%c %Z");

    return oss.str();
}

diff_database::diff_database(const std::string & path) : m_db{ path, true }
{
    ctguard::libs::sqlite::sqlite_statement create_stmt{ "CREATE TABLE IF NOT EXISTS `diskscan-diffdata` ( "
                                                         "`name` VARCHAR(4096) NOT NULL UNIQUE PRIMARY KEY,"
                                                         "`content` TEXT"
                                                         " );",
                                                         m_db };
    create_stmt.run();
}

}  // namespace ctguard::diskscan
