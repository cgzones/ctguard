add_executable (test_logger logger_test.cpp)
target_link_libraries (test_logger PUBLIC ${CMAKE_THREAD_LIBS_INIT})
add_test (Logger test_logger)

add_executable (test_sha256 sha256_test.cpp)
target_link_libraries (test_sha256 PUBLIC external_sha2)
add_test (Sha256 test_sha256 test_sha256)
