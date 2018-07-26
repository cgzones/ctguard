#include "lexer.hpp"

#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace ctguard::libs::config {

lexer::lexer(std::istream & input)
  : m_input{ input }, m_position{ 1, 0 }, m_prev_position{ 0, 0 }, m_next_token{ token::type::unknown, m_position, ' ' }, m_last_char{ ' ' }
{
    (void)get();  // get prime token to initialize m_current_token
}

const token & lexer::peek() const noexcept
{
    return m_next_token;
}

token lexer::get()
{
    token tmp{ std::move(m_next_token) };
    m_next_token = gettok();

    return tmp;
}

char lexer::mygetchar() noexcept
{
    const char ret = static_cast<char>(m_input.get());

    switch (ret) {
        case '\n':
            m_position.line_break();
            break;
        case '\r':
            break;
        default:
            m_position.char_advance();
            break;
    }

    return ret;
}

token lexer::gettok()
{
    // Skip any whitespace.
    while (isspace(m_last_char)) {
        m_last_char = mygetchar();
    }

    m_prev_position = m_position;

    const position startPosition{ std::move(m_position) };  // really copy
    std::string content{};

    // quoted string
    if (m_last_char == '"') {
        while ((m_last_char = mygetchar()) != EOF) {
            if (m_last_char == '"') {
                m_last_char = mygetchar();  // eat "
                return token{ token::type::string, startPosition, content };
            }
            content += m_last_char;
        }
        return token{ token::type::unknown, startPosition, content };
    }
    if (m_last_char == '\'') {
        while ((m_last_char = mygetchar()) != EOF) {
            if (m_last_char == '\'') {
                m_last_char = mygetchar();  // eat '
                return token{ token::type::string, startPosition, content };
            }
            content += m_last_char;
        }
        return token{ token::type::unknown, startPosition, content };
    }

    if (isalpha(m_last_char)) {  // identifier: [a-zA-Z_][a-zA-Z0-9_.]*
        content = m_last_char;
        while (isalnum((m_last_char = mygetchar())) || m_last_char == '_' || m_last_char == '.')
            content += m_last_char;

        return token{ token::type::identifier, startPosition, content };
    }

    if (isdigit(m_last_char)) {  // Integer: [0-9]+
        do {
            content += m_last_char;
            m_last_char = mygetchar();
        } while (isdigit(m_last_char));

        return token{ token::type::integer, startPosition, content };
    }

    if (m_last_char == '#') {
        // Comment until end of line.
        do {
            m_last_char = mygetchar();
        } while (m_last_char != EOF && m_last_char != '\n' && m_last_char != '\r');

        return gettok();
    }

    // Check for end of file.  Don't eat the EOF.
    if (m_last_char == EOF) {
        return token{ token::type::eof, startPosition, content };
    }

    // Otherwise, just return the character as its ascii value.
    content = m_last_char;
    m_last_char = mygetchar();
    return token{ token::type::unknown, startPosition, content };
}

lexer lexer::create(const std::string & input)
{
    std::istringstream is{ input };
    lexer l{ is };

    return l;
}

}  // namespace ctguard::libs::config
