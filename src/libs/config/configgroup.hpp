#pragma once

#include "position.hpp"

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace ctguard::libs::config {

struct config_option
{
    std::string key;
    std::vector<std::string> options;
    position pos;
};

class config_group
{
  public:
    config_group(std::string name, std::string keyword, position pos) : m_name{ std::move(name) }, m_keyword{ std::move(keyword) }, m_position{ std::move(pos) }
    {}

    [[nodiscard]] const std::string & name() const { return m_name; }
    [[nodiscard]] const std::string & keyword() const { return m_keyword; }
    [[nodiscard]] const position & pos() const { return m_position; }

    [[nodiscard]] std::unordered_map<std::string, config_option>::const_iterator begin() const { return m_options.cbegin(); }
    [[nodiscard]] std::unordered_map<std::string, config_option>::const_iterator end() const { return m_options.cend(); }

    [[nodiscard]] const std::set<config_group> & subgroups() const { return m_subconfigs; }

    // for std::set compatibility
    [[nodiscard]] bool operator<(const config_group & other) const { return m_position < other.m_position; }

    friend class parser;

  private:
    std::unordered_map<std::string, config_option> m_options;
    std::set<config_group> m_subconfigs;
    std::string m_name, m_keyword;
    position m_position;
};

} /* namespace ctguard::libs::config */
