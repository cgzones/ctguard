#include <cxxtest/TestSuite.h>

#include <unistd.h>
#include <array>

#include "../libs/sqlite/sqlitedb.hpp"
#include "../libs/sqlite/sqliteexception.hpp"
#include "../libs/sqlite/sqlitestatement.hpp"


using namespace ctguard::libs::sqlite;

class sqlite_testclass : public CxxTest::TestSuite {
private:
    sqlite_db *m_database;
    std::array<char, 256> m_filename;

public:

    virtual void setUp() override {
        ::strncpy(m_filename.data(), "/tmp/tmp_file-XXXXXX", m_filename.size());
        int fd = ::mkstemp(m_filename.data());
        if(fd == -1) {
            throw std::logic_error { "mkstemp" };
        }
        ::close(fd);

        m_database = new sqlite_db { m_filename.data(), true };
    }

    virtual void tearDown() override {
        delete m_database;
        unlink(m_filename.data());
    }

    void test_createdb_fail() {
        TS_ASSERT_THROWS_EQUALS(sqlite_db("/tmp/test.db", false), const sqlite_exception & e, e.what(), "Can not open database '/tmp/test.db': unable to open database file");
    }

    void test_simpleio() {
        sqlite_statement create_table_stmt {
            "CREATE TABLE `test_table`"
            "("
            "PersonID int,"
            "LastName varchar(255),"
            "FirstName varchar(255),"
            "Address varchar(255),"
            "City varchar(255)"
            ");",
            *m_database };
        create_table_stmt.run();

        sqlite_statement {
            "INSERT INTO `test_table`"
            "VALUES"
            "("
            "1701, 'Meier', 'Max', 'Milchstraße 10', 'Galaxis'"
            ");",
            *m_database }.run();

        sqlite_statement get1 {
            "SELECT * FROM `test_table`"
            "WHERE PersonID = 1701;",
            *m_database };
        auto result = get1.run(true);

        std::string last_name, city;
        int id;
        for(const auto & row : result) {
            last_name = row.get<const char *>(1);
            city = row.get<std::string>(4);
            id = row.get<int>(0);
        }

        TS_ASSERT_EQUALS(last_name, "Meier");
        TS_ASSERT_EQUALS(city, "Galaxis");
        TS_ASSERT_EQUALS(id, 1701);


        get1.reset();
        std::string last_name2, city2;
        int id2{ 0 };
        auto result2 = get1.run(true);
        for(const auto & row : result2) {
            std::tie(last_name2, city2, id2) = row.get_columns<const char*, std::string, int>(1, 4, 0);
        }

        TS_ASSERT_EQUALS(last_name2, "Meier");
        TS_ASSERT_EQUALS(city2, "Galaxis");
        TS_ASSERT_EQUALS(id2, 1701);

    }




};
