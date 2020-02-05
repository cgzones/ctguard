#pragma once

#include <optional>
#include <string>

namespace ctguard::libs {

std::optional<std::string> getusername(::uid_t uid);
std::optional<::uid_t> getuserid(const std::string & username);

std::optional<std::string> getgroupname(::gid_t gid);
std::optional<::gid_t> getgroupid(const std::string & groupname);

} /* namespace ctguard::libs */
