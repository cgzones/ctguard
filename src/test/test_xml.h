#include <cxxtest/TestSuite.h>

#include "../libs/libexception.hpp"
#include "../libs/xml/XMLparser.hpp"

using namespace ctguard::libs::xml;

class xml_testclass : public CxxTest::TestSuite
{

#define TS_ASSERT_XML_EQUALS(INPUT, OUTPUT)                                                                                                                    \
    do {                                                                                                                                                       \
        std::istringstream _is{ INPUT };                                                                                                                       \
        XMLparser _p{ _is };                                                                                                                                   \
        XMLNode _root{ _p.parse() };                                                                                                                           \
        std::ostringstream _os;                                                                                                                                \
        _os << _root;                                                                                                                                          \
        TS_ASSERT_EQUALS(_os.str(), OUTPUT);                                                                                                                   \
    } while (0)

#define TS_ASSERT_XML_EQUAL(INPUT)                                                                                                                             \
    do {                                                                                                                                                       \
        TS_ASSERT_XML_EQUALS(INPUT, INPUT);                                                                                                                    \
    } while (0)

#define TS_ASSERT_XML_ERROR(INPUT, ERROR_MSG)                                                                                                                  \
    do {                                                                                                                                                       \
        std::istringstream _is{ INPUT };                                                                                                                       \
        XMLparser _p{ _is };                                                                                                                                   \
        TS_ASSERT_THROWS_EQUALS(_p.parse(), const ctguard::libs::lib_exception & _le, _le.what(), ERROR_MSG);                                                  \
    } while (0)

  public:
    void test_start(void)
    {
        TS_ASSERT_XML_EQUAL("<root></root>");
        TS_ASSERT_XML_EQUALS("<root/>", "<root></root>");
        TS_ASSERT_XML_EQUALS("<root />", "<root></root>");
        TS_ASSERT_XML_EQUAL("<root>test123</root>");
    }

    void test_children(void)
    {
        TS_ASSERT_XML_EQUAL("<root><child></child></root>");
        TS_ASSERT_XML_EQUALS("<root><child><grandchild/></child></root>", "<root><child><grandchild></grandchild></child></root>");
        TS_ASSERT_XML_EQUALS("<root>1<child>2</child>3<child>4</child>5<child/>6</root>", "<root>1356<child>2</child><child>4</child><child></child></root>");
    }

    void test_attributes(void)
    {
        TS_ASSERT_XML_EQUAL("<root id=\"1\"></root>");
        TS_ASSERT_XML_EQUALS("<root id=\"1\" />", "<root id=\"1\"></root>");
        TS_ASSERT_XML_EQUAL("<root id=\"1\" id2=\"2\"></root>");
        TS_ASSERT_XML_EQUAL("<root id=\"test/test\"></root>");
    }

    void test_comment(void)
    {
        TS_ASSERT_XML_EQUALS("<root>1<!--   2    -->3</root>", "<root>13</root>");
        TS_ASSERT_XML_EQUALS("<root>1<!--2-->3</root>", "<root>13</root>");
        TS_ASSERT_XML_EQUALS("<root>1<!---->3</root>", "<root>13</root>");
        TS_ASSERT_XML_EQUALS("<root>1<!--2--><!--2-->3</root>", "<root>13</root>");
        TS_ASSERT_XML_EQUALS("<root <!--description-->>1<!--description-->3</<!--description-->root>", "<root>13</root>");
        TS_ASSERT_XML_EQUALS("<root><!-- comment with - --></root>", "<root></root>");
    }

    void test_escape(void)
    {
        // TS_ASSERT_XML_EQUALS("<root1>\\</root1\\></root1>", "<root1></root1></root1>");
    }

    void test_failures(void)
    {
        // TODO::

        TS_ASSERT_XML_ERROR("<root>", "1:6: Unexpected stream error/end");

        TS_ASSERT_XML_ERROR("<root></root2>", "1:14: Tag 'root' not closed while closing tag 'root2'");

        TS_ASSERT_XML_ERROR("<!-- comment", "1:11: Unexpected stream error/end");

        TS_ASSERT_XML_ERROR("</root>", "1:2: Unknown character (5) '/'");

        TS_ASSERT_XML_ERROR("<root attr=\"attribute></root>", "1:30: Unexpected stream error/end");

        TS_ASSERT_XML_ERROR("<root attr=attribute></root>", "1:12: Unknown character (3) '=', expected '='");

        TS_ASSERT_XML_ERROR("<root attr", "1:11: Unexpected stream error/end");

        TS_ASSERT_XML_ERROR("<root attr=", "1:12: Unknown character (3) '=', expected '='");

        TS_ASSERT_XML_ERROR("<root attr=  test\"test\"/>", "1:12: Unknown character (3) '=', expected '='");

        TS_ASSERT_XML_ERROR("<root attr=\"\"", "1:14: Unexpected stream error/end");

        TS_ASSERT_XML_ERROR("<root attr=\"test\"test/>", "1:24: Unknown character (3) '/', expected '='");

        TS_ASSERT_XML_ERROR("<root attr=\"", "1:13: Unexpected stream error/end");

        TS_ASSERT_XML_ERROR("<root attr=\"test\" attr2=\"test\" attr=\"test123\"></root>", "1:39: Attribute 'attr' already defined");

        TS_ASSERT_XML_ERROR("<root attr></root>", "1:12: Unknown character (3) '>', expected '='");

        TS_ASSERT_XML_ERROR("<root attr attr2=\"test\"></root>", "1:12: Unknown character (3) ' ', expected '='");
    }
};
