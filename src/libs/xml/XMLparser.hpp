#pragma once

#include <istream>
#include <string_view>

#include "XMLNode.hpp"

namespace ctguard::libs::xml {

class XMLparser
{
    enum class parse_state
    {
        initial,
        open_tag_key,
        open_tag_attr,
        body,
        close_tag,
        attr_key,
        attr_value
    };

  public:
    explicit XMLparser(std::istream & input) : m_input{ input } {}

    [[nodiscard]] XMLNode parse();

  private:
    std::istream & m_input;
    static_assert(std::is_unsigned<std::size_t>::value, "signed overflow");
    std::size_t m_line{ 1 }, m_position{ 0 };
    bool m_escaped{ false };

    [[noreturn]] void parse_error(std::string_view message) const;
    [[nodiscard]] char get_char();
    [[nodiscard]] char get_char_low();
    [[nodiscard]] char get_char_raw();
};

} /* namespace ctguard::libs::xml */
