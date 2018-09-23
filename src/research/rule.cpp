#include "rule.hpp"

#include <fstream>
#include <regex>

#include "../libs/errnoexception.hpp"
#include "../libs/libexception.hpp"
#include "../libs/logger.hpp"
#include "../libs/parsehelper.hpp"
#include "../libs/xml/XMLparser.hpp"

namespace ctguard::research {

rule * find_rule(std::vector<rule> & rules, rule_id_t id)
{
    for (auto & rl : rules) {
        if (rl.m_id == id) {
            return &rl;
        }

        rule * child;
        if ((child = find_rule(rl.m_children, id)) != nullptr) {
            return child;
        }
    }

    return nullptr;
}

template<typename T>
static T parse_integral(const std::string & name, const std::string & content)
{
    std::size_t pos;
    unsigned long result_orig;
    try {
        result_orig = std::stoul(content, &pos);
    } catch (const std::exception & e) {
        throw std::out_of_range{ "Invalid " + name + " '" + content + "' given: " + e.what() };
    }
    if (pos != content.length()) {
        throw std::out_of_range{ "Invalid " + name + " '" + content + "' given: no full parse" };
    }
    if (result_orig > std::numeric_limits<T>::max()) {
        throw std::out_of_range{ "Invalid " + name + " '" + content + "' given: value to big" };
    }
    return static_cast<T>(result_orig);
}

static bool parse_bool(const std::string & name, const std::string & content)
{
    if (content == "true" || content == "TRUE" || content == "True") {
        return true;
    }

    if (content == "false" || content == "FALSE" || content == "False") {
        return false;
    }

    throw std::out_of_range{ "Invalid " + name + " bool value given: '" + content + "'" };
}

static rule_match parse_rule_match(const std::string & value)
{
    if (value == "exact") {
        return rule_match::exact;
    } else if (value == "regex") {
        return rule_match::regex;
    } else if (value == "empty") {
        return rule_match::empty;
    } else {
        throw std::out_of_range{ "Invalid rule_match value" };
    }
}

static std::string trim(const std::string & str)
{
    std::string::size_type begin = 0, end = str.size() - 1;
    while (begin < end && std::isspace(str[begin]) != 0) {
        ++begin;
    }

    while (end > begin && std::isspace(str[end]) != 0) {
        --end;
    }

    return str.substr(begin, end - begin + 1);
}

void parse_rules(rule_cfg & rules, const std::string & rules_path)
{
    std::vector<rule> & cfg = rules.std_rules;
    std::vector<rule> & group_rules = rules.group_rules;

    std::ifstream rules_file{ rules_path };
    if (!rules_file.is_open()) {
        throw libs::errno_exception{ "Can not open config file '" + rules_path + "'" };
    }

    libs::xml::XMLparser p{ rules_file };
    libs::xml::XMLNode root = p.parse();

    if (root.name() != "rule_group") {
        throw libs::lib_exception{ "Expected root 'rule_group' node, got '" + root.name() + "'" };
    }

    for (auto const & attr : root.attributes()) {
        throw libs::lib_exception{ "Invalid attribute for root node: '" + attr.first + "'" };
    }

    for (auto const & node : root.children()) {
        if (node.name() == "group") {
            for (auto const & attr : node.attributes()) {
                throw libs::lib_exception{ "Invalid attribute for group node: '" + attr.first + "'" };
            }
            for (auto const & sub_node : node.children()) {
                throw libs::lib_exception{ "Invalid child for group node: '" + sub_node.name() + "'" };
            }
            std::string v{ trim(node.value()) };
            if (v.empty()) {
                throw libs::lib_exception{ "Invalid empty group node" };
            }
            if (v[0] == '!') {
                throw libs::lib_exception{ "Invalid reserved group node '" + v + "'" };
            }
            rules.groups.insert(std::move(v));

        } else if (node.name() == "intervention") {
            for (auto const & attr : node.attributes()) {
                throw libs::lib_exception{ "Invalid attribute for intervention node: '" + attr.first + "'" };
            }
            for (auto const & sub_node : node.children()) {
                throw libs::lib_exception{ "Invalid child for intervention node: '" + sub_node.name() + "'" };
            }
            std::string v{ trim(node.value()) };
            if (v.empty()) {
                throw libs::lib_exception{ "Invalid empty intervention node" };
            }
            rules.interventions.insert(std::move(v));

        } else if (node.name() == "format") {
            format f;
            std::string ireg;

            for (auto const & attr : node.attributes()) {
                if (attr.first == "name") {
                    f.m_name = attr.second;

                } else {
                    throw libs::lib_exception{ "Invalid attribute for format node: '" + attr.first + "'" };
                }
            }

            for (auto const & sub_node : node.children()) {
                if (sub_node.name() == "regex") {
                    for (auto const & attr : sub_node.attributes()) {
                        throw libs::lib_exception{ "Invalid attribute for format node: '" + attr.first + "'" };
                    }
                    ireg += sub_node.value();

                } else if (sub_node.name() == "fields") {
                    for (auto const & attr : sub_node.attributes()) {
                        throw libs::lib_exception{ "Invalid attribute for fields node: '" + attr.first + "'" };
                    }

                    const std::string & fields_str = sub_node.value();

                    std::size_t current, previous = 0;
                    current = fields_str.find(',');
                    while (current != std::string::npos) {
                        f.m_fields.push_back(trim(fields_str.substr(previous, current - previous)));
                        previous = current + 1;
                        current = fields_str.find(',', previous);
                    }
                    f.m_fields.push_back(trim(fields_str.substr(previous, current - previous)));

                } else {
                    throw libs::lib_exception{ "Unsupported format node child: " + sub_node.name() };
                }
            }

            // validate format
            if (f.m_name.empty()) {
                throw libs::lib_exception{ "No name given for format" };
            }
            if (ireg.empty()) {
                throw libs::lib_exception{ "No regex given for format '" + f.m_name + "'" };
            }
            try {
                f.m_reg = ireg;
                if (f.m_reg.mark_count() != f.m_fields.size()) {
                    throw libs::lib_exception{ "Number of regex fields mismatch (" + std::to_string(f.m_reg.mark_count()) +
                                               " != " + std::to_string(f.m_fields.size()) + ")" };
                }
            } catch (const std::exception & e) {
                throw libs::lib_exception{ "Invalid regex '" + ireg + "' for format '" + f.m_name + "': " + e.what() };
            }

            rules.formats.emplace_back(f);

        } else if (node.name() == "rule") {
            rule ex;
            rule_id_t unless_id{ 0 };
            std::string ireg;

            for (auto const & attr : node.attributes()) {
                if (attr.first == "id") {
                    ex.m_id = parse_integral<rule_id_t>(attr.first, attr.second);

                } else if (attr.first == "priority") {
                    ex.m_priority = parse_integral<priority_t>(attr.first, attr.second);

                } else if (attr.first == "always_alert") {
                    try {
                        ex.m_always_alert = libs::parse_bool(attr.second);
                    } catch (const std::exception & e) {
                        throw libs::lib_exception{ "Invalid attribute value '" + attr.second + "' for '" + attr.first + "': " + e.what() };
                    }

                } else {
                    throw libs::lib_exception{ "Invalid attribute for rule node: '" + attr.first + "'" };
                }
            }

            for (auto const & sub_node : node.children()) {
                if (sub_node.name() == "regex") {
                    for (auto const & attr : sub_node.attributes()) {
                        throw libs::lib_exception{ "Invalid attribute for regex node: '" + attr.first + "'" };
                    }
                    ireg += sub_node.value();
                }

                else if (sub_node.name() == "fields") {
                    for (auto const & attr : sub_node.attributes()) {
                        throw libs::lib_exception{ "Invalid attribute for fields node: '" + attr.first + "'" };
                    }

                    const std::string & fields_str = sub_node.value();

                    std::size_t current, previous = 0;
                    current = fields_str.find(',');
                    while (current != std::string::npos) {
                        ex.m_regex_fields.push_back(trim(fields_str.substr(previous, current - previous)));
                        previous = current + 1;
                        current = fields_str.find(',', previous);
                    }
                    ex.m_regex_fields.push_back(trim(fields_str.substr(previous, current - previous)));
                }

                else if (sub_node.name() == "description") {
                    for (auto const & attr : sub_node.attributes()) {
                        throw libs::lib_exception{ "Invalid attribute for description node: '" + attr.first + "'" };
                    }
                    ex.m_description += sub_node.value();
                }

                else if (sub_node.name() == "if_rule") {
                    for (auto const & attr : sub_node.attributes()) {
                        throw libs::lib_exception{ "Invalid attribute for if_rule node: '" + attr.first + "'" };
                    }

                    try {
                        const std::string & ifrule_str = sub_node.value();
                        std::size_t current, previous = 0;
                        current = ifrule_str.find(',');
                        while (current != std::string::npos) {
                            ex.m_parent_ids.push_back(libs::parse_integral<rule_id_t>(trim(ifrule_str.substr(previous, current - previous))));
                            previous = current + 1;
                            current = ifrule_str.find(',', previous);
                        }
                        ex.m_parent_ids.push_back(libs::parse_integral<rule_id_t>(trim(ifrule_str.substr(previous, current - previous))));
                    } catch (const std::exception & e) {
                        throw libs::lib_exception{ "Invalid if_rule setting '" + sub_node.value() + "': " + e.what() };
                    }
                }

                else if (sub_node.name() == "unless_rule") {
                    struct unless_rule ur;
                    for (auto const & attr : sub_node.attributes()) {
                        if (attr.first == "timeout") {
                            ur.timeout = parse_integral<rule_timeout_time_t>(attr.first, attr.second);
                        } else {
                            throw libs::lib_exception{ "Invalid attribute for unless_rule node: '" + attr.first + "'" };
                        }
                    }
                    unless_id = ur.id = parse_integral<rule_id_t>(sub_node.name(), sub_node.value());
                    ex.m_unless_rule = std::move(ur);
                }

                else if (sub_node.name() == "group") {
                    for (auto const & attr : sub_node.attributes()) {
                        throw libs::lib_exception{ "Invalid attribute for group node: '" + attr.first + "'" };
                    }
                    const std::string & group_str = sub_node.value();

                    std::size_t current = group_str.find(','), previous = 0;
                    while (current != std::string::npos) {
                        ex.m_groups.insert(trim(group_str.substr(previous, current - previous)));
                        previous = current + 1;
                        current = group_str.find(',', previous);
                    }
                    ex.m_groups.insert(trim(group_str.substr(previous, current - previous)));
                }

                else if (sub_node.name() == "if_group") {
                    for (auto const & attr : sub_node.attributes()) {
                        throw libs::lib_exception{ "Invalid attribute for if_group node: '" + attr.first + "'" };
                    }
                    ex.m_trigger_group = sub_node.value();
                }

                else if (sub_node.name() == "if_field") {
                    std::string name;
                    rule_match rm = rule_match::exact;
                    for (auto const & attr : sub_node.attributes()) {
                        if (attr.first == "name") {
                            name = attr.second;

                        } else if (attr.first == "match") {
                            try {
                                rm = parse_rule_match(attr.second);
                            } catch (const std::exception & e) {
                                throw libs::lib_exception{ "Invalid attribute match value '" + attr.second + "' : " + e.what() };
                            }

                        } else {
                            throw libs::lib_exception{ "Invalid attribute for if_field node: '" + attr.first + "'" };
                        }
                    }
                    if (name.empty()) {
                        throw libs::lib_exception{ "No name given for if_field node in rule " + std::to_string(ex.m_id) };
                    }
                    ex.m_trigger_fields.emplace(name, std::make_pair(rm, sub_node.value()));
                }

                else if (sub_node.name() == "if_trait") {
                    std::string name;
                    rule_match rm = rule_match::exact;
                    for (auto const & attr : sub_node.attributes()) {
                        if (attr.first == "name") {
                            name = attr.second;

                        } else if (attr.first == "match") {
                            try {
                                rm = parse_rule_match(attr.second);
                            } catch (const std::exception & e) {
                                throw libs::lib_exception{ "Invalid attribute match value '" + attr.second + "' : " + e.what() };
                            }

                        } else {
                            throw libs::lib_exception{ "Invalid attribute for if_trait node: '" + attr.first + "'" };
                        }
                    }
                    if (name.empty()) {
                        throw libs::lib_exception{ "No name given for if_trait node in rule " + std::to_string(ex.m_id) };
                    }
                    ex.m_trigger_traits.emplace(name, std::make_pair(rm, sub_node.value()));
                }

                else if (sub_node.name() == "activation_group") {
                    struct activation_group ag;
                    for (auto const & attr : sub_node.attributes()) {
                        if (attr.first == "time") {
                            ag.time = parse_integral<rule_activation_time_t>(attr.first, attr.second);
                        } else if (attr.first == "rate") {
                            ag.rate = parse_integral<rule_activation_rate_t>(attr.first, attr.second);
                        } else if (attr.first == "reset") {
                            ag.reset = parse_bool(attr.first, attr.second);
                        } else {
                            throw libs::lib_exception{ "Invalid attribute for activation_group node: '" + attr.first + "'" };
                        }
                    }
                    if (ag.time == 0) {
                        throw libs::lib_exception{ "No time given for activation_group node in rule " + std::to_string(ex.m_id) };
                    }
                    if (ag.rate == 0) {
                        throw libs::lib_exception{ "No rate given for activation_group node in rule " + std::to_string(ex.m_id) };
                    }
                    if (ag.rate < 2) {
                        throw libs::lib_exception{ "Invalid rate given for activation_group node in rule id " + std::to_string(ex.m_id) + ": " +
                                                   std::to_string(ag.rate) };
                    }
                    ag.group_name = sub_node.value();
                    ex.m_activation_group = std::move(ag);
                }

                else if (sub_node.name() == "same_field") {
                    for (auto const & attr : sub_node.attributes()) {
                        throw libs::lib_exception{ "Invalid attribute for same_field node: '" + attr.first + "'" };
                    }
                    ex.m_same_field = sub_node.value();
                }

                else if (sub_node.name() == "intervention") {
                    struct intervention_rule ir;
                    for (auto const & attr : sub_node.attributes()) {
                        if (attr.first == "name") {
                            ir.name = attr.second;

                        } else if (attr.first == "field") {
                            ir.field = attr.second;

                        } else if (attr.first == "ignore_empty_field") {
                            ir.ignore_empty_field = parse_bool(attr.first, attr.second);

                        } else {
                            throw libs::lib_exception{ "Invalid attribute for intervention node: '" + attr.first + "'" };
                        }
                    }
                    if (ir.name.empty()) {
                        throw libs::lib_exception{ "No name given for intervention node in rule " + std::to_string(ex.m_id) };
                    }
                    if (ir.field.empty()) {
                        throw libs::lib_exception{ "No field given for intervention node in rule " + std::to_string(ex.m_id) };
                    }
                    if (rules.interventions.find(ir.name) == rules.interventions.end()) {
                        throw libs::lib_exception{ "Invalid intervention name '" + ir.name + "' for rule " + std::to_string(ex.m_id) };
                    }
                    // if (std::find(ex.m_regex_fields.begin(), ex.m_regex_fields.end(), ir.field) == ex.m_regex_fields.end()) {
                    //    throw libs::lib_exception{ "Invalid intervention field '" + ir.field + "' for rule " + std::to_string(ex.m_id) };
                    //}

                    ex.m_intervention_rules.emplace_back(std::move(ir));
                }

                else {
                    throw libs::lib_exception{ "Unsupported rule node child: " + sub_node.name() };
                }
            }

            // checks for valid rule
            if (ex.m_id == 0) {
                throw libs::lib_exception{ "No id given" };
            }
            if (find_rule(cfg, ex.m_id) != nullptr || find_rule(group_rules, ex.m_id) != nullptr) {
                throw libs::lib_exception{ "Duplicate id given: " + std::to_string(ex.m_id) };
            }
            if (ex.m_priority == static_cast<priority_t>(-1)) {
                throw libs::lib_exception{ "No priority given for rule " + std::to_string(ex.m_id) };
            }
            if (!ex.m_parent_ids.empty() && !ex.m_trigger_group.empty()) {
                throw libs::lib_exception{ "if_rule and if_group are not supported together in rule " + std::to_string(ex.m_id) };
            }
            if (!ex.m_same_field.empty() && ex.m_activation_group.group_name.empty()) {
                throw libs::lib_exception{ "same_field only works with activation_group in rule " + std::to_string(ex.m_id) };
            }
            if (ex.m_unless_rule.id != 0 && (find_rule(cfg, ex.m_unless_rule.id) == nullptr && find_rule(group_rules, ex.m_unless_rule.id) == nullptr)) {
                throw libs::lib_exception{ "No such unless_rule in rule " + std::to_string(ex.m_id) };
            }
            if (ex.m_unless_rule.id != 0 && ex.m_parent_ids.empty()) {
                throw libs::lib_exception{ "unless_rule only works with if_rule in rule " + std::to_string(ex.m_id) };
            }
            if (!ireg.empty()) {
                try {
                    ex.m_reg = ireg;
                    if (ex.m_reg->mark_count() != ex.m_regex_fields.size()) {
                        throw libs::lib_exception{ "Number of regex fields mismatch (" + std::to_string(ex.m_reg->mark_count()) +
                                                   " != " + std::to_string(ex.m_regex_fields.size()) + ")" };
                    }
                } catch (const std::exception & e) {
                    throw libs::lib_exception{ "Invalid regex '" + ireg + "' for rule " + std::to_string(ex.m_id) + ": " + e.what() };
                }
            }
            if (!ex.m_trigger_group.empty() && rules.groups.find(ex.m_trigger_group) == rules.groups.end()) {
                if (ex.m_trigger_group != "!ALWAYS") {
                    throw libs::lib_exception{ "Invalid trigger group '" + ex.m_trigger_group + "' for rule " + std::to_string(ex.m_id) };
                }
            }
            if (!ex.m_activation_group.group_name.empty() && rules.groups.find(ex.m_activation_group.group_name) == rules.groups.end()) {
                throw libs::lib_exception{ "Invalid activation group '" + ex.m_activation_group.group_name + "' for rule " + std::to_string(ex.m_id) };
            }
            for (const auto & g : ex.groups()) {
                if (rules.groups.find(g) == rules.groups.end()) {
                    throw libs::lib_exception{ "Invalid group '" + g + "' in rule " + std::to_string(ex.m_id) };
                }
            }
            for (const auto & i : ex.m_parent_ids) {
                if (i == ex.m_id) {
                    throw libs::lib_exception{ "Invalid if_rule id '" + std::to_string(i) + "' in rule " + std::to_string(ex.m_id) };
                }
            }

            // insert rule normally
            if (!ex.m_trigger_group.empty() || !ex.m_activation_group.group_name.empty()) {
                group_rules.emplace_back(std::move(ex));
            } else if (ex.m_parent_ids.empty()) {
                cfg.emplace_back(std::move(ex));
            } else {
                for (const auto & parent_id : ex.m_parent_ids) {
                    rule * parent;
                    if ((parent = find_rule(cfg, parent_id)) != nullptr) {
                        parent->m_children.emplace_back(ex);
                    } else {
                        if ((parent = find_rule(group_rules, parent_id)) != nullptr) {
                            parent->m_children.emplace_back(ex);
                        } else {
                            throw libs::lib_exception{ "Invalid rule trigger '" + std::to_string(parent_id) + "' for rule " + std::to_string(ex.m_id) +
                                                       ": no such rule id" };
                        }
                    }
                }
            }

            // insert rule for unless trigger
            if (unless_id != 0) {
                rule * unless_p;
                if ((unless_p = find_rule(cfg, unless_id)) != nullptr) {
                    unless_p->m_children.emplace_back(std::move(ex));
                } else {
                    if ((unless_p = find_rule(group_rules, unless_id)) != nullptr) {
                        unless_p->m_children.emplace_back(std::move(ex));
                    } else {
                        throw libs::lib_exception{ "Invalid unless_rule trigger '" + std::to_string(unless_id) + "' for rule " + std::to_string(ex.m_id) +
                                                   ": no such rule id" };
                    }
                }
            }

        } else {
            throw libs::lib_exception{ "Unexpected node, got '" + node.name() + "'" };
        }
    }
}

}  // namespace ctguard::research
