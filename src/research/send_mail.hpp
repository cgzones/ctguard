#pragma once

#include <string>

namespace ctguard::research {

void send_mail(const std::string & smtpserver, unsigned short smtpport, const std::string & from, const std::string & to, const std::string & subject,
               const std::string & replyto, const std::string & msg);

}  // namespace ctguard::research
