#include "XMLparser.hpp"

#include "../libexception.hpp"
#include <sstream>

namespace ctguard::libs::xml {

char XMLparser::get_char_raw()
{
    char c = static_cast<char>(m_input.get());

    if (!m_input) {
        parse_error("Unexpected stream error/end");
    }

    if (c == '\n') {
        ++m_line;
        m_position = 0;

        return get_char_raw();
    } else {
        ++m_position;
        return c;
    }
}

char XMLparser::get_char_low()
{
    char c = get_char_raw();

    if (m_escaped && c == '\\') {
        m_escaped = true;
        return get_char_raw();
    } else {
        m_escaped = false;
        return c;
    }
}

char XMLparser::get_char()
{
    char c = get_char_low();

    if (c == '<' && m_input.peek() == '!') {
        m_input.get();
        ++m_position;  // consume '!'

        if ((c = get_char_low()) != '-' || m_input.peek() != '-') {
            parse_error(std::string("Unknown command sequence '") + c + "'");
        }
        m_input.get();
        ++m_position;  // consume second '-'

        while ((c = get_char_low()) != '-' || m_input.peek() != '-')
            ;
        m_input.get();
        ++m_position;  // consume second '-'

        if ((c = get_char_low()) != '>') {
            parse_error(std::string("Expected closing of command sequence instead of '") + c + "'");
        }

        return get_char();
    }

    return c;
}

XMLNode XMLparser::parse()  // @suppress("No return")
{
    std::string name, value, close_name, attr_key, attr_value;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<XMLNode> children;
    parse_state state{ parse_state::initial };

    while (char c = get_char()) {
        switch (state) {
            case parse_state::initial:
                while (std::isspace(c)) {
                    c = get_char();
                }
                if (!m_escaped && c == '<') {
                    state = parse_state::open_tag_key;
                } else {
                    parse_error(std::string("Unknown character (1) '") + c + "'");
                }
                break;
            case parse_state::open_tag_key:
                while (std::isalnum(c) || c == '_') {
                    name += c;
                    c = get_char();
                }
                m_input.unget();
                state = parse_state::open_tag_attr;
                break;
            case parse_state::body:
                while ((m_escaped || c != '<') && (std::isprint(c) || c == '\t')) {
                    if (c != '\t') {
                        value += c;
                    }
                    c = get_char();
                }
                if (!m_escaped && c == '<' && m_input.peek() == '/') {
                    m_input.get();
                    ++m_position;  // consume '/'
                    state = parse_state::close_tag;
                } else if (!m_escaped && c == '<') {
                    m_input.unget();
                    children.push_back(parse());
                } else {
                    parse_error(std::string("Unknown character (2) '") + c + "'");
                }
                break;
            case parse_state::attr_key:
                while (std::isalnum(c) || c == '_') {
                    attr_key += c;
                    c = get_char();
                }
                for (const auto & attrs : attributes) {
                    if (attrs.first == attr_key) {
                        parse_error("Attribute '" + attr_key + "' already defined");
                    }
                }
                if (!m_escaped && c == '=' && m_input.peek() == '"') {
                    m_input.get();
                    ++m_position;  // consume '"'
                    state = parse_state::attr_value;
                } else {
                    parse_error(std::string("Unknown character (3) '") + c + "', expected '='");
                }
                break;
            case parse_state::attr_value:
                while ((m_escaped || c != '"') && std::isprint(c)) {
                    attr_value += c;
                    c = get_char();
                }
                if (!m_escaped && c == '"') {
                    attributes.emplace_back(std::move(attr_key), std::move(attr_value));
                    state = parse_state::open_tag_attr;
                } else {
                    parse_error(std::string("Unknown character (4) '") + c + "'");
                }
                break;
            case parse_state::open_tag_attr:
                while (std::isspace(c)) {
                    c = get_char();
                }
                if (std::isalnum(c)) {
                    state = parse_state::attr_key;
                    m_input.unget();
                } else if (!m_escaped && c == '/' && m_input.peek() == '>') {
                    m_input.get();
                    ++m_position;  // consume '>'
                    return XMLNode{ name, attributes };
                } else if (!m_escaped && c == '>') {
                    state = parse_state::body;
                } else {
                    parse_error(std::string("Unknown character (5) '") + c + "'");
                }
                break;
            case parse_state::close_tag:
                while (std::isalnum(c) || c == '_') {
                    close_name += c;
                    c = get_char();
                }
                if (!m_escaped && c == '>') {
                    if (name != close_name) {
                        parse_error("Tag '" + name + "' not closed while closing tag '" + close_name + "'");
                    }
                    return XMLNode{ name, attributes, value, children };
                } else {
                    parse_error(std::string("Unknown character (6) '") + c + "'");
                }
        }
    }

    parse_error("Unexpected end, while in tag '" + name + "'");
}

void XMLparser::parse_error(std::string_view message) const
{
    std::ostringstream os;
    os << m_line << ":" << (m_position - 1) << ": " << message;
    throw lib_exception{ os.str() };
}

}  // namespace ctguard::libs::xml
