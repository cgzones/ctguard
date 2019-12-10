#pragma once

#include "configgroup.hpp"
#include "lexer.hpp"

namespace ctguard::libs::config {

class parser
{
  public:
    explicit parser(std::istream & input);

    [[nodiscard]] config_group parse();

    parser(const parser & other) = delete;
    parser & operator=(const parser & other) = delete;
    parser(const parser && other) = delete;
    parser & operator=(const parser && other) = delete;

  private:
    lexer m_lexer;

    [[nodiscard]] config_group parse_group(std::string name, std::string keyword, bool need_close = false, position pos = { 1, 0 });
};

} /* namespace ctguard::libs::config */
