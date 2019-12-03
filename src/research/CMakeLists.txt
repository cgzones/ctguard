add_executable (ctguard-research
                                 config.cpp
                                 config.hpp
                                 daemon.cpp
                                 daemon.hpp
                                 event.cpp
                                 event.hpp
                                 intervention_sink.cpp
                                 intervention_sink.hpp
                                 process_log.cpp
                                 process_log.hpp
                                 research.cpp
                                 research.hpp
                                 rule.cpp
                                 rule.hpp
                                 send_mail.cpp
                                 send_mail.hpp
                                 )

target_link_libraries (ctguard-research PUBLIC
                                               libs
                                               libs_config
                                               libs_xml
                                               ${CMAKE_THREAD_LIBS_INIT}
                                               )

install (TARGETS ctguard-research DESTINATION /usr/sbin)
