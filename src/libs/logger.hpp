#pragma once

#include <ctime>
#include <iomanip>
#include <sstream>
#include <unistd.h>

namespace ctguard::libs {

// Adopted from http://www.drdobbs.com/cpp/logging-in-c/201804215

enum class log_level
{
    ERROR,
    WARNING,
    INFO,
    DEBUG,
    DEBUG1,
    DEBUG2,
    DEBUG3,
    DEBUG4
};

// std::ostream &operator<<(std::ostream &out, log_level ll);

inline std::ostream & operator<<(std::ostream & out, log_level ll)
{
    switch (ll) {
        case log_level::ERROR:
            out << "ERROR ";
            break;
        case log_level::WARNING:
            out << "WARN  ";
            break;
        case log_level::INFO:
            out << "INFO  ";
            break;
        case log_level::DEBUG:
            out << "DEBUG ";
            break;
        case log_level::DEBUG1:
            out << "DEBUG1";
            break;
        case log_level::DEBUG2:
            out << "DEBUG2";
            break;
        case log_level::DEBUG3:
            out << "DEBUG3";
            break;
        case log_level::DEBUG4:
            out << "DEBUG4";
            break;
    }

    return out;
}

template<typename OutputPolicy>
class Log
{
  public:
    Log() = default;
    Log(const Log & other) = delete;
    Log & operator=(const Log & other) = delete;
    Log(Log && other) = delete;
    Log & operator=(Log && other) = delete;
    ~Log() noexcept;

    std::ostringstream & get(log_level level = log_level::INFO);

    static log_level & reporting_level() noexcept
    {
        static log_level logLevel{ log_level::INFO };
        return logLevel;
    }

    static const char *& domain() noexcept
    {
        static const char * domain{ "unset" };
        return domain;
    }

    static bool & printprefix() noexcept
    {
        static bool printprefix{ true };
        return printprefix;
    }

  private:
    std::ostringstream m_os;
};

template<typename OutputPolicy>
std::ostringstream & Log<OutputPolicy>::get(log_level level)
{
    if (Log::printprefix()) {
        char time_buffer[32];
        const std::time_t now = std::time(nullptr);
        struct tm time
        {};

        std::strftime(time_buffer, sizeof(time_buffer), "%c %Z", ::localtime_r(&now, &time));
        m_os << time_buffer;
        m_os << ' ' << std::setw(16) << Log::domain();

        m_os << ' ' << std::setw(5) << ::getpid();
        m_os << ' ';
    }

    if (reporting_level() >= log_level::DEBUG) {
        m_os << ::pthread_self();
        m_os << ' ';
    }

    m_os << level << ": ";
    return m_os;
}

template<typename OutputPolicy>
Log<OutputPolicy>::~Log() noexcept
{
    m_os << '\n';

    OutputPolicy::output(m_os.str());
}

class Output2FILE
{  // implementation of OutputPolicy
  public:
    static FILE *& stream() noexcept;
    static void output(const std::string & msg) noexcept;
};

inline FILE *& Output2FILE::stream() noexcept
{
    static FILE * pStream = ::stderr;
    return pStream;
}

inline void Output2FILE::output(const std::string & msg) noexcept
{
    FILE * pStream = stream();

    ::fprintf(pStream, "%s", msg.c_str());
    ::fflush(pStream);
}

using FILELog = Log<Output2FILE>;

} /* namespace ctguard::libs */

#define FILE_LOG(level)                                                                                                                                        \
    if ((level) > ctguard::libs::FILELog::reporting_level())                                                                                                   \
        ;                                                                                                                                                      \
    else                                                                                                                                                       \
        ctguard::libs::FILELog().get(level)
