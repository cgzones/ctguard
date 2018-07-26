#pragma once

#include <iostream>
#include <string>

#include "token.hpp"

namespace ctguard::libs::config {

class lexer
{
  public:
    explicit lexer(std::istream & input);
    [[nodiscard]] const position & get_position() const { return m_position; }
    [[nodiscard]] const position & get_prev_position() const { return m_prev_position; }
    [[nodiscard]] token get();
    [[nodiscard]] const token & peek() const noexcept;

    [[nodiscard]] static lexer create(const std::string & input);

    lexer(const lexer & other) = delete;
    lexer & operator=(const lexer & other) = delete;
    lexer(lexer && other) = default;
    lexer & operator=(lexer && other) = delete;

  private:
    std::istream & m_input;
    position m_position, m_prev_position;
    token m_next_token;
    char m_last_char;

    [[nodiscard]] char mygetchar() noexcept;
    [[nodiscard]] token gettok();
};

}  // namespace ctguard::libs::config
