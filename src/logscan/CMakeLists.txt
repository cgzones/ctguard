add_executable (ctguard-logscan
                                config.cpp
                                config.hpp
                                daemon.cpp
                                daemon.hpp
                                eventsink.cpp
                                eventsink.hpp
                                logfile.hpp
                                logscan.cpp
                                )

target_link_libraries (ctguard-logscan PUBLIC
                                               libs
                                               libs_config
                                               ${CMAKE_THREAD_LIBS_INIT}
                                               )

install (TARGETS ctguard-logscan DESTINATION /usr/sbin)
