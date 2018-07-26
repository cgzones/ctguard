#include "eventsink.hpp"

#include "../libs/errnoexception.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/types/memory.hpp>

#include <unistd.h>

namespace ctguard::diskscan {

sink::~sink() {}

file_sink::file_sink(const std::string & path) : m_out{ path, std::ios::app }
{
    if (!m_out.is_open()) {
        throw libs::errno_exception{ "Can not open '" + path + "'" };
    }
}

void file_sink::send(libs::source_event se)
{
    m_out << " [" << se.source_domain << "] : " << se.message << "\n";
    m_out.flush();
}

socket_sink::socket_sink(std::string path, bool unit_test) : m_client{ std::move(path) }, m_unit_test{ unit_test }
{
    std::array<char, 1024> buffer;
    ::gethostname(buffer.data(), buffer.size());
    m_hostname = buffer.data();
}

void socket_sink::send(libs::source_event se)
{
    if (m_unit_test) {
        se.time_send = 1;
        se.hostname = "unittest";
    } else {
        se.time_send = std::time(nullptr);
        se.hostname = m_hostname;
    }

    std::stringstream ss;
    {
        cereal::BinaryOutputArchive oarchive(ss);
        oarchive(std::move(se));
    }

    m_client.send(ss.str());
}

}  // namespace ctguard::diskscan
