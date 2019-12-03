add_executable (ctguard-diskscan
                                 config.cpp
                                 config.hpp
                                 daemon.cpp
                                 daemon.hpp
                                 database.cpp
                                 database.h
                                 diskscan.cpp
                                 eventsink.cpp
                                 eventsink.hpp
                                 filedata.cpp
                                 filedata.hpp
                                 watch.cpp
                                 watch.hpp
                                 )

target_link_libraries (ctguard-diskscan PUBLIC
                                               external_dtl
                                               external_sha2
                                               libs
                                               libs_config
                                               libs_sqlite
                                               ${CMAKE_THREAD_LIBS_INIT}
                                               )

install (TARGETS ctguard-diskscan DESTINATION /usr/sbin)
