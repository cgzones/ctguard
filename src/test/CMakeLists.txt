add_executable (test_logger logger_test.cpp)
target_link_libraries (test_logger PUBLIC ${CMAKE_THREAD_LIBS_INIT})
add_test (Logger test_logger)

add_executable (test_sha256 sha256_test.cpp)
target_link_libraries (test_sha256 PUBLIC external_sha2)
add_test (Sha256 test_sha256 test_sha256)

add_executable (test_xml xml_test.cpp)
configure_file (test.xml test.xml COPYONLY)
target_link_libraries (test_xml PUBLIC libs libs_xml)
add_test (Xml test_xml test.xml)
