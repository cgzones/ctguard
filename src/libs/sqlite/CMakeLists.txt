add_library (libs_sqlite STATIC
                                sqlitedb.cpp
                                sqlitedb.hpp
                                sqliteexception.cpp
                                sqliteexception.hpp
                                sqlitestatement.cpp
                                sqlitestatement.hpp
                                sqlitetransaction.hpp
                                )

find_package (SQLite3 REQUIRED)
target_include_directories (libs_sqlite PRIVATE ${SQLite3_INCLUDE_DIRS})
target_link_libraries (libs_sqlite ${SQLite3_LIBRARIES})
