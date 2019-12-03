file (COPY
    test1
    test2
    test3
    test4
    test5
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
    )

add_test (NAME Research1 COMMAND test.sh WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test1)
add_test (NAME Research2 COMMAND test.sh WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test2)
add_test (NAME Research3 COMMAND test.sh WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test3)
add_test (NAME Research4 COMMAND test.sh WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test4)
add_test (NAME Research5 COMMAND test.sh WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test5)

set_tests_properties (Research1 Research2 Research3 Research4 Research5 PROPERTIES TIMEOUT 30 ENVIRONMENT TZ=UTC)
