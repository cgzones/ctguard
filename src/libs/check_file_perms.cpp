#include "check_file_perms.hpp"

#include "errnoexception.hpp"
#include "libexception.hpp"
#include "safe_utilities.hpp"

#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>  // geteuid/getegid

namespace ctguard::libs {

void check_cfg_file_perms(const std::string & path)
{
    struct stat info;
    if (::stat(path.c_str(), &info) == -1) {
        throw errno_exception{ "Can not stat" };
    }

    unsigned ctguard_gid{ 0 };
    {
        const auto groupid = getgroupid("ctguard");
        if (groupid) {
            ctguard_gid = *groupid;
        }
    }

    if (info.st_uid != 0 && info.st_uid != ::geteuid()) {
        throw lib_exception{ "Insecure owner: " + std::to_string(info.st_uid) };
    }

    if (info.st_gid != 0 && info.st_gid != ::getegid() && info.st_gid != ctguard_gid) {
        throw lib_exception{ "Insecure group: " + std::to_string(info.st_gid) };
    }

    if (info.st_mode & S_IWGRP) {
        throw lib_exception{ "Is group writeable" };
    }

    if (info.st_mode & S_IWOTH) {
        throw lib_exception{ "Is other writeable" };
    }

    if (info.st_mode & S_IROTH) {
        throw lib_exception{ "Is other readable" };
    }
}

}  // namespace ctguard::libs
