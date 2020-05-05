#include "filedata.hpp"

#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <sstream>

#include "../libs/errnoexception.hpp"
#include "../libs/logger.hpp"
#include "../libs/safe_utilities.hpp"
#include "../libs/scopeguard.hpp"
#include <dtl/dtl.hpp>

#include "../libs/sqlite/sqlitestatement.hpp"

namespace ctguard::diskscan {

static std::vector<std::string> split(const std::string & str, char delim)
{
    std::vector<std::string> cont;
    std::size_t previous = 0;
    std::size_t current = str.find(delim);
    while (current != std::string::npos) {
        cont.push_back(str.substr(previous, current - previous));
        previous = current + 1;
        current = str.find(delim, previous);
    }
    cont.push_back(str.substr(previous, current - previous));
    return cont;
}

std::unique_ptr<file_data> file_data_factory::construct(std::string path, bool check_content, bool check_diff)
{
    if (path.empty()) {
        FILE_LOG(libs::log_level::ERROR) << "Empty path given to file_data_factory::construct()";
        return nullptr;
    }

    std::unique_ptr<file_data> fdp = std::make_unique<file_data>(file_data::_constructor_tag{});

    fdp->m_path = std::move(path);

    struct stat s;  // NOLINT(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
    if (lstat(fdp->m_path.c_str(), &s) == -1) {
        if (errno == ENOENT) {
            FILE_LOG(libs::log_level::DEBUG) << "Object seems to be deleted: '" << fdp->m_path << "'";
        } else {
            FILE_LOG(libs::log_level::ERROR) << "Can not stat object '" << fdp->m_path << "': " << ::strerror(errno);
        }

        fdp->m_exists = false;
        fdp->m_content_checked = true;
        fdp->m_user = "null";
        fdp->m_group = "null";
        fdp->m_size = 0;
        fdp->m_mode = 0;
        fdp->m_inode = 0;
        fdp->m_mtime = 0;
        fdp->m_ctime = 0;
        fdp->m_type = file_data::type::unknown;
        fdp->m_sha256[0] = '\0';
        fdp->m_target = "";
        fdp->m_dead_lnk = false;

        return fdp;
    }

    FILE_LOG(libs::log_level::DEBUG) << "Computing file_data for '" << fdp->m_path << "': " << std::boolalpha << check_content << " " << check_diff;

    fdp->m_exists = true;

    {
        auto username = libs::getusername(s.st_uid);
        if (username) {
            fdp->m_user = std::move(*username);
        } else {
            FILE_LOG(libs::log_level::ERROR) << "No user with uid '" << s.st_uid << "'";
            fdp->m_user = "!unknown";
        }
    }

    {
        auto groupname = libs::getgroupname(s.st_gid);
        if (groupname) {
            fdp->m_group = std::move(*groupname);
        } else {
            FILE_LOG(libs::log_level::ERROR) << "No group with gid '" << s.st_gid << "'";
            fdp->m_group = "!unknown";
        }
    }

    fdp->m_size = s.st_size;
    fdp->m_mode = s.st_mode;
    fdp->m_inode = s.st_ino;
    fdp->m_mtime = s.st_mtime;
    fdp->m_ctime = s.st_ctime;

    switch (s.st_mode & S_IFMT) {  // NOLINT(hicpp-signed-bitwise)
        case S_IFBLK:
            fdp->m_type = file_data::type::block_device;
            break;
        case S_IFCHR:
            fdp->m_type = file_data::type::character_device;
            break;
        case S_IFDIR:
            fdp->m_type = file_data::type::directory;
            break;
        case S_IFIFO:
            fdp->m_type = file_data::type::fifo;
            break;
        case S_IFLNK:
            fdp->m_type = file_data::type::link;
            break;
        case S_IFREG:
            fdp->m_type = file_data::type::file;
            break;
        case S_IFSOCK:
            fdp->m_type = file_data::type::socket;
            break;
        default:
            fdp->m_type = file_data::type::unknown;
            FILE_LOG(libs::log_level::ERROR) << "Unknown file type for '" << fdp->m_path << "': " << std::hex << s.st_mode;
            break;
    }

    if (check_content && fdp->m_type == file_data::type::file) {
        bool compute_diff = check_diff && fdp->m_size < m_max_diff_size;
        std::string db_content;
        std::stringstream ocontent;
        bool found_content = false;

        const int file = ::open(fdp->m_path.c_str(), O_RDONLY | O_CLOEXEC);  // NOLINT(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
        if (file == -1) {
            if (errno == EACCES) {
                FILE_LOG(libs::log_level::WARNING) << "Can not open file '" << fdp->m_path << "': " << ::strerror(errno);
            } else {
                FILE_LOG(libs::log_level::ERROR) << "Can not open file '" << fdp->m_path << "': " << ::strerror(errno);
            }
            return fdp;
        }

        libs::scope_guard sg{ [file]() { ::close(file); } };

        if (compute_diff) {
            libs::sqlite::sqlite_statement select{ "SELECT `content` FROM `diskscan-diffdata` WHERE `name` = $001;", m_db };
            select.bind(1, fdp->m_path);
            const auto & ret = select.run(true);

            for (const auto column : ret) {
                if (found_content) {
                    FILE_LOG(libs::log_level::ERROR) << "Multiple diffdb entries for '" << fdp->m_path << '\'';
                }
                found_content = true;

                db_content = column.get<std::string_view>(0);
            }
        }

        {
            ssize_t ret;  // NOLINT(cppcoreguidelines-init-variables)
            SHA256_CTX ctx;
            SHA256_Init(&ctx);

            std::array<unsigned char, 16384> buf;  // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)

            while ((ret = ::read(file, buf.data(), buf.size())) > 0) {
                SHA256_Update(&ctx, buf.data(), static_cast<size_t>(ret));
                if (compute_diff) {
                    if (memchr(buf.data(), '\0', static_cast<size_t>(ret)) != nullptr) {
                        FILE_LOG(libs::log_level::DEBUG) << "File '" << fdp->m_path << "' has binary format";
                        compute_diff = false;
                    } else {
                        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                        ocontent << std::string_view{ reinterpret_cast<const char *>(buf.data()), static_cast<size_t>(ret) };
                        // oss.write(&buffer.data()[0], n);
                    }
                }
            }

            if (ret == -1) {
                FILE_LOG(libs::log_level::ERROR) << "Can not read from file '" << fdp->m_path << "': " << ::strerror(errno);
                return fdp;
            }

            SHA256_End(&ctx, fdp->m_sha256.data());
        }

        if (compute_diff) {
            const std::string content{ ocontent.str() };
            if (found_content) {
                libs::sqlite::sqlite_statement update{ "UPDATE `diskscan-diffdata` SET `content` = $001 WHERE `name` = $002;", m_db };
                update.bind(1, content);
                update.bind(2, fdp->m_path);
                update.run();
            } else {
                libs::sqlite::sqlite_statement insert{ "INSERT INTO `diskscan-diffdata` ( `name`, `content` ) VALUES ( $001, $002 );", m_db };
                insert.bind(1, fdp->m_path);
                insert.bind(2, content);
                insert.run();
            }

            if (db_content.empty()) {
                FILE_LOG(libs::log_level::DEBUG) << "File '" << fdp->m_path << "' has no stored content";
            } else {
                dtl::Diff<std::string> diff{ split(db_content, '\n'), split(content, '\n') };
                diff.onHuge();
                diff.compose();
                diff.composeUnifiedHunks();

                std::ostringstream oss;
                diff.printUnifiedFormat(oss);
                fdp->m_diff = oss.str();
            }
        }

        fdp->m_content_checked = true;
    }

    else if (check_content && fdp->m_type == file_data::type::link) {
        struct stat tmp;  // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
        if (::stat(fdp->m_path.c_str(), &tmp) == -1) {
            if (errno == ENOENT) {
                FILE_LOG(libs::log_level::DEBUG) << "stat(" << fdp->m_path << ") failed, cause link destination is dead";
            } else {
                FILE_LOG(libs::log_level::ERROR) << "Can not stat link target of '" << fdp->m_path << "': " << ::strerror(errno);
            }

            fdp->m_dead_lnk = true;
        } else {
            FILE_LOG(libs::log_level::DEBUG2) << "stat(" << fdp->m_path << ") succeeded";
            fdp->m_dead_lnk = false;
        }

        std::array<char, 256> buf;  // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
        ssize_t ret;                // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init,cppcoreguidelines-init-variables)
        if ((ret = ::readlink(fdp->m_path.c_str(), buf.data(), buf.size())) < 0) {
            FILE_LOG(libs::log_level::ERROR) << "Can not readlink for '" << fdp->m_path << "': " << ::strerror(errno);
            return fdp;
        }
        if (static_cast<size_t>(ret) + 1 >= buf.size()) {
            FILE_LOG(libs::log_level::WARNING) << "Target of link '" << fdp->m_path << "' is longer than " << buf.size()
                                               << " characters; will countinue truncated";
            buf[buf.size() - 1] = '\0';
        } else {
            buf[static_cast<size_t>(ret)] = '\0';  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }

        fdp->m_target = buf.data();

        fdp->m_content_checked = true;
    }

    return fdp;
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
    if (S_ISREG(m)) {
        return '-';
    }
    if (S_ISDIR(m)) {
        return 'd';
    }
    if (S_ISBLK(m)) {
        return 'b';
    }
    if (S_ISCHR(m)) {
        return 'c';
    }
    if (S_ISFIFO(m)) {
        return 'p';
    }
    if (S_ISLNK(m)) {
        return 'l';
    }
    if (S_ISSOCK(m)) {
        return 's';
    }
    return '?';
}

std::string print_mode(file_data::mode_type m)
{
    std::ostringstream oss;

    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    oss << mode_to_type(m) << ((m & S_IRUSR) ? 'r' : '-') << ((m & S_IWUSR) ? 'w' : '-')
        << ((m & S_ISUID) ? (m & S_IXUSR) ? 's' : 'S' : ((m & S_IXUSR) ? 'x' : '-')) << ((m & S_IRGRP) ? 'r' : '-') << ((m & S_IWGRP) ? 'w' : '-')
        << ((m & S_ISGID) ? ((m & S_IXGRP) ? 's' : 'l') : ((m & S_IXGRP) ? 'x' : '-')) << ((m & S_IROTH) ? 'r' : '-') << ((m & S_IWOTH) ? 'w' : '-')
        << ((m & S_ISVTX) ? ((m & S_IXOTH) ? 't' : 'T') : ((m & S_IXOTH) ? 'x' : '-'));

    return oss.str();
}

std::string print_time(time_t t)
{
    struct tm ts;  // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
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

} /* namespace ctguard::diskscan */
