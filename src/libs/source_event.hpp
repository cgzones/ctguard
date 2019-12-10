#pragma once

#include <chrono>
#include <string>

namespace ctguard::libs {

class source_event
{
    using sevent_time_t = std::time_t;

  public:
    source_event() = default;

    std::string hostname;
    std::string source_program;
    std::string source_domain;
    std::string message;
    bool control_message{ false };
    sevent_time_t time_scanned{ 0 }, time_send{ 0 };

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive(hostname, source_program, source_domain, message, control_message, time_scanned, time_send);
    }
};

} /* namespace ctguard::libs */
