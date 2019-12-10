#include <cxxtest/TestSuite.h>

#include <sstream>

#include "../libs/config/parseexception.hpp"
#include "../libs/config/parser.hpp"

class config_testclass : public CxxTest::TestSuite
{
  public:
    void test_parse_simple(void)
    {
        std::stringstream ss;
        ss << "test = success\n";

        ctguard::libs::config::parser p{ ss };
        auto global = p.parse();
        TS_ASSERT_EQUALS(global->name(), "global");
        TS_ASSERT_EQUALS(global->size(), 1);
        for (auto & c : *global) {
            TS_ASSERT_EQUALS(c.first, "test");
            TS_ASSERT_EQUALS(c.second, "success");
        }
    }

    void test_parse_comment(void)
    {
        std::stringstream ss;
        ss << "test = success\n"
           << "#blah = blah\n"
           << "test2 = \"123\"\n";

        ctguard::libs::config::parser p{ ss };
        auto global = p.parse();
        TS_ASSERT_EQUALS(global->size(), 2);
        TS_ASSERT_EQUALS(global->at("test"), "success");
        TS_ASSERT_EQUALS(global->at("test2"), "123");
    }

    void test_parse_duplicate_key(void)
    {
        std::stringstream ss;
        ss << "test = success\n"
           << "test = \"123\"\n";

        ctguard::libs::config::parser p{ ss };
        TS_ASSERT_THROWS_EQUALS(p.parse(), const ctguard::libs::config::parse_exception & pe, pe.what(), "2:1: key 'test' already defined");
    }

    void test_parse_subgroup_simple(void)
    {
        std::stringstream ss;
        ss << "test {\n"
           << "sub = test2"
           << "}";

        ctguard::libs::config::parser p{ ss };
        auto global = p.parse();
        TS_ASSERT_EQUALS(global->name(), "global");
        TS_ASSERT_EQUALS(global->size(), 0);
        TS_ASSERT_EQUALS(global->sub_size(), 1);
        for (auto & c : global->subgroups()) {
            TS_ASSERT_EQUALS(c.first, "test@");
            TS_ASSERT_EQUALS(c.second.get()->name(), "test");
            TS_ASSERT_EQUALS(c.second.get()->keyword(), "");
            TS_ASSERT_EQUALS(c.first, "test@");
            TS_ASSERT_EQUALS(c.second->size(), 1);
            TS_ASSERT_EQUALS(c.second->at("sub"), "test2");
        }
    }

    void test_parse_subgroup_nonclose(void)
    {
        std::stringstream ss;
        ss << "test {\n"
           << "sub = test2";

        ctguard::libs::config::parser p{ ss };
        TS_ASSERT_THROWS_EQUALS(p.parse(), const ctguard::libs::config::parse_exception & pe, pe.what(), "2:12: config group 'test' not closed");
    }

    void test_parse_subsubgroup_simple(void)
    {
        std::stringstream ss;
        ss << "sup {\n"
           << "sub1 {\n"
           << "sub2 = test2\n"
           << "}"
           << "}";

        ctguard::libs::config::parser p{ ss };
        std::unique_ptr<ctguard::libs::config::config_group> global_ptr{ p.parse() };
        ctguard::libs::config::config_group & global{ *global_ptr };
        TS_ASSERT_EQUALS(global.size(), 0);
        TS_ASSERT_EQUALS(global.sub_size(), 1);
        TS_ASSERT_EQUALS(global.subgroups().at("sup@")->subgroups().at("sub1@")->at("sub2"), "test2");
        TS_ASSERT_EQUALS(global["sup@"]["sub1@"]("sub2"), "test2");
        TS_ASSERT_THROWS_EQUALS(global["fail1"], const std::out_of_range & e, e.what(), "no config group named 'fail1' in group 'global'");
        TS_ASSERT_THROWS_EQUALS(global("fail2"), const std::out_of_range & e, e.what(), "no config variable named 'fail2' in group 'global'");
    }

    void test_parse_namedsubgroup_simple(void)
    {
        std::stringstream ss;
        ss << "test key {\n"
           << "sub = test2"
           << "}";

        ctguard::libs::config::parser p{ ss };
        auto global = p.parse();
        TS_ASSERT_EQUALS(global->name(), "global");
        TS_ASSERT_EQUALS(global->size(), 0);
        TS_ASSERT_EQUALS(global->sub_size(), 1);
        for (auto & c : global->subgroups()) {
            TS_ASSERT_EQUALS(c.first, "test@key");
            TS_ASSERT_EQUALS(c.second->size(), 1);
            TS_ASSERT_EQUALS(c.second->name(), "test");
            TS_ASSERT_EQUALS(c.second->keyword(), "key");
            TS_ASSERT_EQUALS(c.second->at("sub"), "test2");
        }
    }

    void test_parse_namedsubgroup_twice(void)
    {
        std::stringstream ss;
        ss << "test key1 {\n"
           << "sub1 = test1"
           << "}"
           << "test key2 {"
           << "sub2 = test2"
           << "}";

        ctguard::libs::config::parser p{ ss };
        auto global = p.parse();
        TS_ASSERT_EQUALS(global->name(), "global");
        TS_ASSERT_EQUALS(global->size(), 0);
        TS_ASSERT_EQUALS(global->sub_size(), 2);

        TS_ASSERT_EQUALS(global->subgroups().find("test@key1")->first, "test@key1");
        TS_ASSERT_EQUALS(global->subgroups().find("test@key1")->second->name(), "test");
        TS_ASSERT_EQUALS(global->subgroups().find("test@key1")->second->keyword(), "key1");
        TS_ASSERT_EQUALS(global->subgroups().find("test@key1")->second->size(), 1);
        TS_ASSERT_EQUALS(global->subgroups().find("test@key1")->second->at("sub1"), "test1");

        TS_ASSERT_EQUALS(global->subgroups().find("test@key2")->first, "test@key2");
        TS_ASSERT_EQUALS(global->subgroups().find("test@key2")->second->name(), "test");
        TS_ASSERT_EQUALS(global->subgroups().find("test@key2")->second->keyword(), "key2");
        TS_ASSERT_EQUALS(global->subgroups().find("test@key2")->second->size(), 1);
        TS_ASSERT_EQUALS(global->subgroups().find("test@key2")->second->at("sub2"), "test2");
    }
};
