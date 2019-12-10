#include "intervention_sink.hpp"

#include <unistd.h>

#include <cereal/archives/binary.hpp>
#include <cereal/types/memory.hpp>

#include "../libs/errnoexception.hpp"

namespace ctguard::research {

sink::~sink() noexcept = default;

file_sink::file_sink(const std::string & path) : m_out{ path, std::ios::app }
{
    if (!m_out.is_open()) {
        throw libs::errno_exception{ "Can not open '" + path + "'" };
    }
}

void file_sink::send(const libs::intervention_cmd & cmd)
{
    m_out << " [" << cmd.name << "] : " << cmd.argument << "\n";
    m_out.flush();
}

socket_sink::socket_sink(std::string path) : m_client{ std::move(path) } {}

void socket_sink::send(const libs::intervention_cmd & cmd)
{
    std::stringstream ss;
    {
        cereal::BinaryOutputArchive oarchive(ss);
        oarchive(cmd);
    }

    m_client.send(ss.str());
}

} /* namespace ctguard::research */
