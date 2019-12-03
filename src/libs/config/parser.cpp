#include "parser.hpp"

#include <sstream>

#include "parseexception.hpp"

namespace ctguard::libs::config {

parser::parser(std::istream & input) : m_lexer{ lexer{ input } } {}

config_group parser::parse()
{
    return parse_group("global", "");
}

config_group parser::parse_group(std::string name, std::string keyword, bool need_close, position pos)  // @suppress("No return")
{
    config_group cg{ std::move(name), std::move(keyword), std::move(pos) };

    for (;;) {
        position start_pos = m_lexer.get_prev_position();
        token t{ m_lexer.get() };

        if (t.get_type() == token::type_t::string || t.get_type() == token::type_t::identifier) {
            std::string new_name{ t.get_content() };

            {
                std::unordered_map<std::string, config_option>::const_iterator iter;
                if ((iter = cg.m_options.find(new_name)) != cg.m_options.end()) {
                    throw parse_exception{ t.get_position(), "key '" + new_name + "' already defined at " + to_string(iter->second.pos) };
                }
            }

            t = m_lexer.get();
            if (t == '{') {
                const auto r = cg.m_subconfigs.emplace(parse_group(new_name, "", true, start_pos));
                if (!r.second) {
                    throw parse_exception{ t.get_position(), "can not insert configgroup '" + new_name + "'" };
                }
                continue;
            } else if (t.get_type() == token::type_t::string || t.get_type() == token::type_t::identifier) {
                std::string new_keyword{ t.get_content() };
                const token & tp = m_lexer.peek();
                if (tp == '{') {
                    t = m_lexer.get();
                    if (t != '{') {
                        throw parse_exception{ t.get_position(), "expected '{' instead of '" + t.get_content() + '\'' };
                    }

                    bool found = false;
                    position found_pos{ 0, 0 };
                    for (const auto & sg : cg.subgroups()) {
                        if (sg.keyword() == new_keyword) {
                            found = true;
                            found_pos = sg.pos();
                            break;
                        }
                    }
                    if (found) {
                        throw parse_exception{ t.get_position(), "subgroup " + new_keyword + " already defined at " + to_string(found_pos) };
                    }
                    const auto r = cg.m_subconfigs.emplace(parse_group(new_name, new_keyword, true, start_pos));
                    if (!r.second) {
                        throw parse_exception{ t.get_position(), "can not insert configgroup '" + new_name + "'" };
                    }
                    continue;
                } else {
                    const auto r = cg.m_subconfigs.emplace(new_name, new_keyword, start_pos);
                    if (!r.second) {
                        throw parse_exception{ t.get_position(), "can not insert configgroup '" + new_name + "'" };
                    }
                    continue;
                }
            } else if (t != "=") {
                throw parse_exception{ t.get_position(), "expected '=' instead of '" + t.get_content() + '\'' };
            }

            t = m_lexer.get();
            const auto t_line = t.get_position().line_number();
            if (t.get_type() != token::type_t::string && t.get_type() != token::type_t::identifier && t.get_type() != token::type_t::integer) {
                throw parse_exception{ t.get_position(), "expected a value instead of '" + t.get_content() + '\'' };
            }
            std::vector<std::string> options;
            options.emplace_back(std::move(t.get_content()));

            const token & tp = m_lexer.peek();
            while ((tp.get_type() == token::type_t::string || tp.get_type() == token::type_t::identifier || tp.get_type() == token::type_t::integer) &&
                   tp.get_position().line_number() == t_line) {
                t = m_lexer.get();
                options.emplace_back(std::move(t.get_content()));
            }

            config_option co{ new_name, options, start_pos };
            const auto r = cg.m_options.emplace(new_name, std::move(co));
            if (!r.second) {
                throw parse_exception{ start_pos, "can not insert configoption '" + new_name + "'" };
            }
            continue;
        }

        if (need_close && t == '}')
            return cg;

        if (t.get_type() == token::type_t::eof) {
            if (need_close) {
                throw parse_exception{ t.get_position(), "config group '" + cg.name() + "' not closed" };
            }
            return cg;
        }

        throw parse_exception{ t.get_position(), "unknown token '" + t.get_content() + '\'' };
    }
}

}  // namespace ctguard::libs::config
