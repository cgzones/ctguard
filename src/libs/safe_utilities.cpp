#include "safe_utilities.hpp"

#include "errnoexception.hpp"

#include <vector>

#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

namespace ctguard::libs {

std::optional<std::string> getusername(::uid_t uid)
{
    auto bufsize = ::sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufsize == -1) {
        bufsize = 16384;
    }

    std::vector<char> buf;
    buf.resize(static_cast<std::size_t>(bufsize));

    struct ::passwd pwd;
    struct ::passwd * result;
    const auto s = getpwuid_r(uid, &pwd, buf.data(), buf.size(), &result);
    if (result == nullptr) {
        if (s == 0) {
            return std::nullopt;
        }

        errno = s;
        throw libs::errno_exception{ "Can not get user details for uid " + uid };
    }

    return pwd.pw_name;
}

std::optional<::uid_t> getuserid(const std::string & username)
{
    auto bufsize = ::sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufsize == -1) {
        bufsize = 16384;
    }

    std::vector<char> buf;
    buf.resize(static_cast<std::size_t>(bufsize));

    struct ::passwd pwd;
    struct ::passwd * result;
    const auto s = getpwnam_r(username.c_str(), &pwd, buf.data(), buf.size(), &result);
    if (result == nullptr) {
        if (s == 0) {
            return std::nullopt;
        }

        errno = s;
        throw libs::errno_exception{ "Can not get user details for username '" + username + "'" };
    }

    return pwd.pw_uid;
}

std::optional<std::string> getgroupname(::gid_t gid)
{
    auto bufsize = ::sysconf(_SC_GETGR_R_SIZE_MAX);
    if (bufsize == -1) {
        bufsize = 16384;
    }

    std::vector<char> buf;
    buf.resize(static_cast<std::size_t>(bufsize));

    struct ::group grp;
    struct ::group * result;
    const auto s = getgrgid_r(gid, &grp, buf.data(), buf.size(), &result);
    if (result == nullptr) {
        if (s == 0) {
            return std::nullopt;
        }

        errno = s;
        throw libs::errno_exception{ "Can not get group details for gid " + gid };
    }

    return grp.gr_name;
}

std::optional<::gid_t> getgroupid(const std::string & groupname)
{
    auto bufsize = ::sysconf(_SC_GETGR_R_SIZE_MAX);
    if (bufsize == -1) {
        bufsize = 16384;
    }

    std::vector<char> buf;
    buf.resize(static_cast<std::size_t>(bufsize));

    struct ::group grp;
    struct ::group * result;
    const auto s = getgrnam_r(groupname.c_str(), &grp, buf.data(), buf.size(), &result);
    if (result == nullptr) {
        if (s == 0) {
            return std::nullopt;
        }

        errno = s;
        throw libs::errno_exception{ "Can not get group details for groupname '" + groupname + "'" };
    }

    return grp.gr_gid;
}

} /* namespace ctguard::libs */
