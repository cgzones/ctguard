#include <cxxtest/TestSuite.h>

#include "../libs/scopeguard.hpp"
#include <functional>

class scopeguard_testclass : public CxxTest::TestSuite
{
  public:
    void test_lambda1(void)
    {
        int i = 0;

        {
            auto x = ctguard::libs::make_scope_guard([&i]() { ++i; });
            TS_ASSERT_EQUALS(i, 0);
        }

        TS_ASSERT_EQUALS(i, 1);
    }

    void test_lambda2(void)
    {
        int i = 0;

        {
            auto x = ctguard::libs::make_scope_guard([i]() mutable { ++i; });
            TS_ASSERT_EQUALS(i, 0);
        }

        TS_ASSERT_EQUALS(i, 0);
    }

    void test_functionobject(void)
    {
        class a
        {
          private:
            int & j_;

          public:
            a(int & j) : j_{ j } {}
            void operator()() { ++j_; }
        };

        int i = 0;

        {
            auto x = ctguard::libs::make_scope_guard(a{ i });
            TS_ASSERT_EQUALS(i, 0);
        }

        TS_ASSERT_EQUALS(i, 1);
    }
};
