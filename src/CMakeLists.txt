find_package (Threads REQUIRED)

find_package(Cereal REQUIRED)
add_compile_definitions (CEREAL_THREAD_SAFE=1)


add_subdirectory (diskscan)
add_subdirectory (external)
add_subdirectory (intervention)
add_subdirectory (libs)
add_subdirectory (logscan)
add_subdirectory (research)
add_subdirectory (test)
