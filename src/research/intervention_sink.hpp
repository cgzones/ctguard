#pragma once

#include <fstream>
#include <memory>
#include <string>

#include "../libs/intervention.hpp"
#include "../libs/source_event.hpp"
#include "../libs/unix_socket_client.hpp"
#include "config.hpp"

namespace ctguard::research {

class sink
{
  public:
    virtual ~sink() noexcept;
    virtual void send(const libs::intervention_cmd & cmd) = 0;
};

class file_sink final : public sink
{
  public:
    explicit file_sink(const std::string & path);
    virtual void send(const libs::intervention_cmd & cmd) override;

  private:
    std::ofstream m_out;
};

class socket_sink final : public sink
{
  public:
    explicit socket_sink(std::string path);
    virtual void send(const libs::intervention_cmd & cmd) override;

  private:
    libs::unix_socket_client m_client;
};

class intervention_sink
{
  public:
    intervention_sink(std::string path, intervention_kind_t ok)
    {
        switch (ok) {
            case intervention_kind_t::FILE:
                m_sink = std::make_unique<file_sink>(std::move(path));
                break;
            case intervention_kind_t::SOCKET:
                m_sink = std::make_unique<socket_sink>(std::move(path));
                break;
        }
    }

    void send(const libs::intervention_cmd & cmd) { m_sink->send(cmd); }

  private:
    std::unique_ptr<sink> m_sink;
};

} /* namespace ctguard::research */
