#pragma once

#include <signal.h>
#include <string>

[[deprecated]] void daemonize(const std::string & pidfile, const std::string & user, const std::string & group, sighandler_t signal_handler, bool foreground);
