#pragma once

#include <fstream>
#include <memory>
#include <string>

#include "../libs/source_event.hpp"
#include "../libs/unix_socket_client.hpp"
#include "config.hpp"

namespace ctguard::logscan {

class sink
{
  public:
    virtual ~sink();
    virtual void send(libs::source_event) = 0;
};

class file_sink final : public sink
{
  public:
    explicit file_sink(const std::string & path);
    virtual void send(libs::source_event se) override;

  private:
    std::ofstream m_out;
};

class socket_sink final : public sink
{
  public:
    socket_sink(std::string path, bool unit_test);
    virtual void send(libs::source_event se) override;

  private:
    libs::unix_socket_client m_client;
    std::string m_hostname;
    bool m_unit_test;
};

class eventsink
{
  public:
    eventsink(std::string path, output_kind_t ok, bool unit_test)
    {
        switch (ok) {
            case output_kind_t::FILE:
                m_sink = std::make_unique<file_sink>(std::move(path));
                break;
            case output_kind_t::SOCKET:
                m_sink = std::make_unique<socket_sink>(std::move(path), unit_test);
                break;
        }
    }

    void send(libs::source_event se) { m_sink->send(std::move(se)); }

  private:
    std::unique_ptr<sink> m_sink;
};

}  // namespace ctguard::logscan
