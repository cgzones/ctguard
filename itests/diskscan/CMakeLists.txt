file (COPY
    test1
    test2
    test3
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
    )

add_test (NAME Diskscan1 COMMAND test.sh WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test1)
add_test (NAME Diskscan2 COMMAND test.sh WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test2)
add_test (NAME Diskscan3 COMMAND test.sh WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test3)

set_tests_properties (Diskscan1 Diskscan2 Diskscan3 PROPERTIES TIMEOUT 30 ENVIRONMENT TZ=UTC)
