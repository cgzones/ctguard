#include "send_mail.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../libs/errnoexception.hpp"
#include "../libs/libexception.hpp"
#include "../libs/scopeguard.hpp"
#include <iomanip>

namespace ctguard::research {

static void send_cmd(int socket, const std::string & cmd, const std::string & return_code)
{
    std::array<char, 2048> buffer;

    if (::send(socket, cmd.c_str(), cmd.length(), 0) == -1) {
        throw libs::errno_exception{ "Can not send command '" + cmd + "'" };
    }

    const ssize_t ret = ::recv(socket, buffer.data(), buffer.size(), 0);
    if (ret < 0) {
        throw libs::errno_exception{ "Can not receive response for command '" + cmd };
    }

    if (static_cast<size_t>(ret) >= buffer.size() - 1) {
        throw libs::lib_exception{ "Receive buffer for command '" + cmd + "' too short: " + std::to_string(ret) + "/" + std::to_string(buffer.size()) };
    }

    buffer[static_cast<std::size_t>(ret)] = '\0';

    if (return_code.compare(0, return_code.length(), buffer.data(), 0, return_code.length()) != 0) {
        throw libs::lib_exception{ "Command '" + cmd + "' does not return '" + return_code + "', instead it returns '" + buffer.data() + "'" };
    }
}

static void send_mail_message(int socket, const std::string & from, const std::string & to, const std::string & subject, const std::string & replyto,
                              const std::string & msg, const std::string & return_code)
{
    std::ostringstream oss;

    oss << "From: " << from << "\r\nTo: " << to << "\r\nSubject: " << subject << "\r\nX-Mailer: ctguard-notifier\r\nReply-To: " << replyto << "\r\n\r\n";
    const std::string envelope{ oss.str() };
    if (::send(socket, envelope.c_str(), envelope.length(), 0) == -1) {
        throw libs::errno_exception{ "Can not send evelope '" + envelope + "'" };
    }

    if (::send(socket, msg.c_str(), msg.length(), 0) == -1) {
        throw libs::errno_exception{ "Can not send message '" + msg + "'" };
    }

    const std::string terminator{ "\r\n.\r\n" };
    if (::send(socket, terminator.c_str(), terminator.length(), 0) == -1) {
        throw libs::errno_exception{ "Can not send the message terminator" };
    }

    std::array<char, 256> buffer;
    const ssize_t ret = recv(socket, buffer.data(), buffer.size(), 0);
    if (ret < 0) {
        throw libs::errno_exception{ "Can not receive message id" };
    }

    if (static_cast<size_t>(ret) >= buffer.size() - 1) {
        throw libs::lib_exception{ "Receive buffer for mail message data too short: " + std::to_string(ret) + "/" + std::to_string(buffer.size()) };
    }

    buffer[static_cast<std::size_t>(ret)] = '\0';

    if (return_code.compare(0, return_code.length(), buffer.data(), 0, return_code.length()) != 0) {
        throw libs::lib_exception{ "Mail message data does not return '" + return_code + "', instead it returns '" + buffer.data() + "'" };
    }
}

void send_mail(const std::string & smtpserver, unsigned short smtpport, const std::string & from, const std::string & to, const std::string & subject,
               const std::string & replyto, const std::string & msg)
{
    struct hostent * host = ::gethostbyname(smtpserver.c_str());
    if (host == nullptr) {
        throw libs::lib_exception{ "Can not resolve hostname '" + smtpserver + "': " + ::hstrerror(h_errno) };
    }

    struct in_addr in;
    ::memcpy(&in, host->h_addr_list[0], static_cast<size_t>(host->h_length));

    const int socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket == -1) {
        throw libs::errno_exception{ "Can not create a socket" };
    }
    libs::scope_guard sg{ [socket]() { ::close(socket); } };

    struct sockaddr_in sa;
    ::memset(&sa, 0, sizeof(sa));
    sa.sin_addr = in;
    sa.sin_family = static_cast<sa_family_t>(host->h_addrtype);
    sa.sin_port = htons(smtpport);
    if (::connect(socket, reinterpret_cast<struct sockaddr *>(&sa), sizeof(sa)) == -1) {
        throw libs::errno_exception{ "Can not connect to '" + smtpserver };
    }

    {
        std::array<char, 4096> buffer;
        if (::recv(socket, buffer.data(), buffer.size(), 0) == -1) {
            throw libs::errno_exception{ "Can not receive welcome message" };
        }
    }

    {
        std::array<char, 512> buffer_hostname;
        ::gethostname(buffer_hostname.data(), buffer_hostname.size());

        send_cmd(socket, std::string("EHLO ") + buffer_hostname.data() + "\r\n", "250");
    }

    send_cmd(socket, "MAIL From:<" + from + ">\r\n", "250");
    send_cmd(socket, "RCPT To:<" + to + ">\r\n", "250");
    send_cmd(socket, "DATA\r\n", "354");
    send_mail_message(socket, from, to, subject, replyto, msg, "250");
    send_cmd(socket, "QUIT\r\n", "221");
}

}  // namespace ctguard::research
