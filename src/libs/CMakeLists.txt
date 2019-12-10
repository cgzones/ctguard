add_subdirectory (config)
add_subdirectory (filesystem)
add_subdirectory (sqlite)
add_subdirectory (xml)

add_library (libs STATIC
                         blockedqueue.hpp
                         check_file_perms.cpp
                         check_file_perms.hpp
                         daemon_helper.cpp
                         daemon_helper.hpp
                         errnoexception.cpp
                         errnoexception.hpp
                         intervention.hpp
                         libexception.cpp
                         libexception.hpp
                         logger.hpp
                         parsehelper.hpp
                         safe_utilities.cpp
                         safe_utilities.hpp
                         scopeguard.hpp
                         source_event.hpp
                         unix_socket_client.cpp
                         unix_socket_client.hpp
                         )

configure_file (ctguard_config.hpp.in ctguard_config.hpp)
