add_executable (ctguard-intervention
                                     config.cpp
                                     config.hpp
                                     daemon.cpp
                                     daemon.hpp
                                     intervention.cpp
                                     )

target_link_libraries (ctguard-intervention PUBLIC
                                                   libs
                                                   libs_config
                                                   ${CMAKE_THREAD_LIBS_INIT}
                                                   )

install (TARGETS ctguard-intervention DESTINATION /usr/sbin)
