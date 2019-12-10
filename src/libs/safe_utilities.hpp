#pragma once

#include <optional>
#include <string>

namespace ctguard::libs {

[[nodiscard]] std::optional<std::string> getusername(::uid_t uid);
[[nodiscard]] std::optional<::uid_t> getuserid(const std::string & username);

[[nodiscard]] std::optional<std::string> getgroupname(::gid_t gid);
[[nodiscard]] std::optional<::gid_t> getgroupid(const std::string & groupname);

} /* namespace ctguard::libs */
