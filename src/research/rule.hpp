#pragma once

#include "config.hpp"
#include "event.hpp"

#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace ctguard::research {

class rule;
class format;
struct rule_cfg
{
    std::vector<rule> std_rules;
    std::vector<rule> group_rules;
    std::set<std::string> groups;
    std::vector<format> formats;
    std::set<std::string> interventions;
};

using rule_activation_time_t = unsigned short;
using rule_activation_rate_t = unsigned short;
using rule_timeout_time_t = unsigned short;

struct activation_group
{
    rule_activation_time_t time{ 0 };
    rule_activation_rate_t rate{ 0 };
    std::string group_name;
    bool reset{ false };
};

struct unless_rule
{
    rule_id_t id{ 0 };
    rule_timeout_time_t timeout{ 0 };
};

enum class rule_match
{
    exact,
    regex,
    empty
};

class rule
{
  public:
    rule() = default;

    const std::optional<std::regex> & reg() const { return m_reg; }
    const std::string & description() const { return m_description; }
    const std::vector<std::string> & regex_fields() const { return m_regex_fields; }
    const std::vector<rule> & children() const { return m_children; }
    priority_t priority() const { return m_priority; }
    const std::set<std::string> groups() const { return m_groups; }
    rule_id_t id() const { return m_id; }
    const std::vector<rule_id_t> & parent_ids() const { return m_parent_ids; }
    bool always_alert() const { return m_always_alert; }
    const std::string & trigger_group() const { return m_trigger_group; }
    const std::map<std::string, std::pair<rule_match, std::string>> & trigger_fields() const { return m_trigger_fields; }
    const std::map<std::string, std::pair<rule_match, std::string>> & trigger_traits() const { return m_trigger_traits; }
    const struct activation_group & activation_group() const { return m_activation_group; }
    const std::string & same_field() const { return m_same_field; }
    const struct unless_rule & unless_rule() const { return m_unless_rule; }
    const std::vector<struct intervention_rule> & interventions() const noexcept { return m_intervention_rules; }

  private:
    rule_id_t m_id{ 0 };
    std::vector<rule_id_t> m_parent_ids;
    priority_t m_priority{ static_cast<priority_t>(-1) };
    bool m_always_alert{ false };
    std::string m_description;
    std::set<std::string> m_groups;
    std::optional<std::regex> m_reg;
    std::vector<std::string> m_regex_fields;

    std::vector<rule> m_children;

    std::string m_trigger_group;
    std::map<std::string, std::pair<rule_match, std::string>> m_trigger_fields;
    std::map<std::string, std::pair<rule_match, std::string>> m_trigger_traits;

    struct activation_group m_activation_group;
    std::string m_same_field;

    struct unless_rule m_unless_rule;

    std::vector<struct intervention_rule> m_intervention_rules;

    friend void parse_rules(rule_cfg & rules, const std::string & rules_path);
    friend rule * find_rule(std::vector<rule> & rules, rule_id_t id);
};

class format
{
  public:
    format() = default;

    const std::string & name() const { return m_name; }
    const std::regex & reg() const { return m_reg; }
    const std::vector<std::string> & fields() const { return m_fields; }

  private:
    std::string m_name;
    std::regex m_reg;
    std::vector<std::string> m_fields;

    friend void parse_rules(rule_cfg & rules, const std::string & rules_path);
};

void parse_rules(rule_cfg & rules, const std::string & rules_path);

}  // namespace ctguard::research
