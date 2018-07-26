#pragma once

#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "../libs/source_event.hpp"
#include "config.hpp"

namespace ctguard ::research {

class rule;
using rule_id_t = unsigned;
struct intervention_rule
{
    std::string name;
    std::string field;
    bool ignore_empty_field{ false };
};

class event
{
  public:
    event() = default;
    explicit event(const libs::source_event & se);

    const std::string & logstr() const { return m_logstr; }

    std::unordered_map<std::string, std::string> & fields() { return m_fields; }
    std::unordered_map<std::string, std::string> & traits() { return m_traits; }
    const std::unordered_map<std::string, std::string> & fields() const { return m_fields; }
    const std::unordered_map<std::string, std::string> & traits() const { return m_traits; }
    const std::set<std::string> groups() const { return m_groups; }
    void add_groups(const std::set<std::string> & input);
    std::string groups_2_str() const;
    void priority(priority_t p) { m_priority = p; }
    priority_t priority() const { return m_priority; }
    void always_alert(bool b) { m_always_alert = b; }
    bool always_alert() const { return m_always_alert; }
    rule_id_t rule_id() const { return m_rule_id; }
    void rule_id(rule_id_t i) { m_rule_id = i; }
    bool control_message() const { return m_control_message; }
    const std::string & description() const { return m_description; }
    void description(std::string description) { m_description = std::move(description); }
    void interventions(std::vector<struct intervention_rule> interventions) { m_intervention_rules = std::move(interventions); }
    const std::vector<intervention_rule> & interventions() const { return m_intervention_rules; }

    event update(const rule & rule) const;

  private:
    std::string m_logstr, m_description;
    bool m_control_message{ false };
    bool m_always_alert{ false };
    std::unordered_map<std::string, std::string> m_fields, m_traits;
    std::set<std::string> m_groups;
    priority_t m_priority{ 0 };
    rule_id_t m_rule_id{ 0 };
    std::vector<struct intervention_rule> m_intervention_rules;

    friend event make_unless_event();
};

}  // namespace ctguard::research
