#include "safe_utilities.hpp"

#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <vector>

#include "errnoexception.hpp"

namespace ctguard::libs {

constexpr static unsigned default_bufsize = 16384;

std::optional<std::string> getusername(::uid_t uid)
{
    auto bufsize = ::sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufsize == -1) {
        bufsize = default_bufsize;
    }

    std::vector<char> buf;
    buf.resize(static_cast<std::size_t>(bufsize));

    struct ::passwd pwd;       // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
    struct ::passwd * result;  // NOLINT(cppcoreguidelines-init-variables)
    const auto s = getpwuid_r(uid, &pwd, buf.data(), buf.size(), &result);
    if (result == nullptr) {
        if (s == 0) {
            return std::nullopt;
        }

        errno = s;
        throw libs::errno_exception{ "Can not get user details for uid " + std::to_string(uid) };
    }

    return pwd.pw_name;
}

std::optional<::uid_t> getuserid(const std::string & username)
{
    auto bufsize = ::sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufsize == -1) {
        bufsize = default_bufsize;
    }

    std::vector<char> buf;
    buf.resize(static_cast<std::size_t>(bufsize));

    struct ::passwd pwd;       // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
    struct ::passwd * result;  // NOLINT(cppcoreguidelines-init-variables)
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
        bufsize = default_bufsize;
    }

    std::vector<char> buf;
    buf.resize(static_cast<std::size_t>(bufsize));

    struct ::group grp;       // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
    struct ::group * result;  // NOLINT(cppcoreguidelines-init-variables)
    const auto s = getgrgid_r(gid, &grp, buf.data(), buf.size(), &result);
    if (result == nullptr) {
        if (s == 0) {
            return std::nullopt;
        }

        errno = s;
        throw libs::errno_exception{ "Can not get group details for gid " + std::to_string(gid) };
    }

    return grp.gr_name;
}

std::optional<::gid_t> getgroupid(const std::string & groupname)
{
    auto bufsize = ::sysconf(_SC_GETGR_R_SIZE_MAX);
    if (bufsize == -1) {
        bufsize = default_bufsize;
    }

    std::vector<char> buf;
    buf.resize(static_cast<std::size_t>(bufsize));

    struct ::group grp;       // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
    struct ::group * result;  // NOLINT(cppcoreguidelines-init-variables)
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
