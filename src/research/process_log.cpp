#include "process_log.hpp"

#include "../libs/libexception.hpp"
#include <iostream>
#include <regex>

namespace ctguard::research {

static std::tuple<bool, std::vector<std::pair<std::string, std::string>>, std::vector<std::pair<std::string, std::string>>> check_rule(
  const event & ev, const rule & rl, std::map<rule_id_t, struct rule_state> & rules_state, bool verbose)
{
    bool match_something{ false };
    bool is_active{ false };
    std::vector<std::pair<std::string, std::string>> modified_fields, modified_traits;

    if (rl.unless_rule().id != 0) {
        std::lock_guard<std::mutex> lg{ rules_state[rl.id()].mutex };

        bool is_child{ false };
        for (const auto & i : rl.parent_ids()) {
            if (ev.rule_id() == i) {
                is_child = true;
                break;
            }
        }
        if (is_child) {
            rules_state[rl.id()].unless_triggered = std::time(nullptr);
            rules_state[rl.id()].unless_timeout = rl.unless_rule().timeout;
            rules_state[rl.id()].unless_event = ev.update(rl);
        } else if (ev.rule_id() == rl.unless_rule().id) {
            rules_state[rl.id()].unless_triggered = 0;
        } else {
            std::ostringstream oss;
            for (const auto & i : rl.parent_ids()) {
                oss << i << " ";
            }
            throw libs::lib_exception{ "Unknown child case: " + std::to_string(ev.rule_id()) + "(event) " + oss.str() + "(parent) " +
                                       std::to_string(rl.unless_rule().id) + "(unless)" };
        }

        return { false, modified_fields, modified_traits };
    }

    // activation_group
    if (!rl.activation_group().group_name.empty()) {
        bool found_activation_group{ false };
        for (const std::string & ev_group : ev.groups()) {
            if (ev_group == rl.activation_group().group_name) {
                found_activation_group = true;
                match_something = true;
                break;
            }
        }

        const auto & iter = rules_state.find(rl.id());
        if (iter == rules_state.end()) {
            if (verbose)
                std::cout << "init rstate|";

            if (found_activation_group) {
                rules_state[rl.id()].mevents.emplace(std::time(nullptr), ev);
            }

        } else {
            std::lock_guard<std::mutex> lg{ iter->second.mutex };
            auto & saved_events{ iter->second.mevents };
            if (found_activation_group) {
                saved_events.emplace(std::time(nullptr), ev);
            }

            // delete too old entries
            {
                const auto current_time = std::time(nullptr);
                while (!saved_events.empty()) {
                    const auto & i = saved_events.begin();
                    if (i->first + rl.activation_group().time < current_time) {
                        saved_events.erase(i);
                    } else {
                        // break, cause elemetns are sorted by time
                        break;
                    }
                }
            }

            if (saved_events.size() >= rl.activation_group().rate) {
                if (rl.same_field().empty()) {
                    is_active = true;
                    std::ostringstream otriggers;
                    for (const auto & evs : saved_events) {
                        otriggers << evs.second.logstr() << '\n';
                    }
                    modified_traits.emplace_back("trigger_logs", otriggers.str());

                    if (verbose)
                        std::cout << "active(" << saved_events.size() << "/" << rl.activation_group().rate << ")|";
                } else {
                    // count same fields
                    const auto & actual_field = ev.fields().find(rl.same_field());
                    if (actual_field != ev.fields().end()) {
                        rule_activation_rate_t same_rate{ 0 };
                        std::ostringstream otriggers;
                        for (const auto & i : saved_events) {
                            const auto & stored_field = i.second.fields().find(rl.same_field());
                            if (stored_field != i.second.fields().end() && actual_field->second == stored_field->second) {
                                same_rate++;
                                otriggers << i.second.logstr() << '\n';
                            }
                        }
                        if (same_rate >= rl.activation_group().rate) {
                            is_active = true;
                            modified_traits.emplace_back("trigger_same_logs", otriggers.str());
                            if (verbose)
                                std::cout << "active_s(" << same_rate << "/" << rl.activation_group().rate << ")|";
                        } else if (verbose)
                            std::cout << "inactive_s(" << same_rate << "/" << rl.activation_group().rate << ")|";
                    } else if (verbose)
                        std::cout << "inactive_s(no field)|";
                }
            } else if (verbose)
                std::cout << "inactive(" << iter->second.mevents.size() << "/" << rl.activation_group().rate << ")|";
        }
    } else {
        is_active = true;
    }

    if (!rl.trigger_group().empty()) {
        match_something = true;

        if (rl.trigger_group() == "!ALWAYS") {
            if (verbose)
                std::cout << "group always match|";
        } else {
            bool found_group = false;
            for (const auto & iter : ev.groups()) {
                if (iter == rl.trigger_group()) {
                    found_group = true;
                    break;
                }
            }
            if (!found_group) {
                if (verbose)
                    std::cout << "not matching (group)\n";
                return { false, modified_fields, modified_traits };
            }
            if (verbose)
                std::cout << "group match|";
        }
    }

    if (!rl.trigger_fields().empty()) {
        match_something = true;
        for (const auto & iter : rl.trigger_fields()) {
            if (verbose)
                std::cout << "field '" + iter.first + "' matching...|";
            bool found = false;
            for (const auto & field : ev.fields()) {
                if (field.first == iter.first) {
                    found = true;
                    switch (iter.second.first) {
                        case rule_match::exact:
                            if (field.second != iter.second.second) {
                                if (verbose)
                                    std::cout << "field '" << iter.first << "' exact mismatch\n";
                                return { false, modified_fields, modified_traits };
                            }
                            break;
                        case rule_match::empty:
                            if (field.second != "") {
                                if (verbose)
                                    std::cout << "field '" << iter.first << "' not empty\n";
                                return { false, modified_fields, modified_traits };
                            }
                            break;
                        case rule_match::regex:
                            std::smatch match;
                            const std::regex regex{ iter.second.second };
                            if (!std::regex_search(field.second, match, regex)) {
                                if (verbose)
                                    std::cout << "field '" << iter.first << "' regex mismatch\n";
                                return { false, modified_fields, modified_traits };
                            }
                            break;
                    }
                }
            }
            if (!found && iter.second.first != rule_match::empty) {
                if (verbose)
                    std::cout << "field '" << iter.first << "' not found\n";
                return { false, modified_fields, modified_traits };
            }
        }
    }

    if (!rl.trigger_traits().empty()) {
        match_something = true;
        for (const auto & iter : rl.trigger_traits()) {
            if (verbose)
                std::cout << "trait '" + iter.first + "' matching...|";
            bool found = false;
            for (const auto & field : ev.traits()) {
                if (field.first == iter.first) {
                    found = true;
                    switch (iter.second.first) {
                        case rule_match::exact:
                            if (field.second != iter.second.second) {
                                if (verbose)
                                    std::cout << "trait '" << iter.first << "' exact mismatch\n";
                                return { false, modified_fields, modified_traits };
                            }
                            break;
                        case rule_match::empty:
                            if (field.second != "") {
                                if (verbose)
                                    std::cout << "field '" << iter.first << "' not empty\n";
                                return { false, modified_fields, modified_traits };
                            }
                            break;
                        case rule_match::regex:
                            std::smatch match;
                            const std::regex regex{ iter.second.second };
                            if (!std::regex_search(field.second, match, regex)) {
                                if (verbose)
                                    std::cout << "trait '" << iter.first << "' regex mismatch\n";
                                return { false, modified_fields, modified_traits };
                            }
                            break;
                    }
                }
            }
            if (!found && iter.second.first != rule_match::empty) {
                if (verbose)
                    std::cout << "trait '" << iter.first << "' not found\n";
                return { false, modified_fields, modified_traits };
            }
        }
    }

    if (rl.reg().has_value()) {
        match_something = true;
        std::smatch match;
        const std::string & to_match = ev.fields().find("log") != ev.fields().end() ? ev.fields().find("log")->second : ev.logstr();
        if (!std::regex_search(to_match, match, *rl.reg())) {
            if (verbose)
                std::cout << "no regex match\n";
            return { false, modified_fields, modified_traits };
        }

        if (verbose)
            std::cout << "regex match|";

        for (size_t i = 1; i < match.size(); ++i) {
            if (match[i] != std::string("")) {
                modified_fields.emplace_back(rl.regex_fields()[i - 1], match[i]);
            }
        }
    }

    if (!rl.parent_ids().empty()) {
        match_something = true;
    }

    if (verbose)
        std::cout << "full match: " << std::boolalpha << (match_something && is_active) << "(" << match_something << " && " << is_active << ")\n";

    return { match_something && is_active, modified_fields, modified_traits };
}

static void update_event(event & ev, const rule & rl, std::vector<std::pair<std::string, std::string>> & modified_fields,
                         std::vector<std::pair<std::string, std::string>> & modified_traits)
{
    ev.description(rl.description());
    ev.always_alert(rl.always_alert());
    ev.add_groups(rl.groups());
    ev.priority(rl.priority());
    ev.rule_id(rl.id());
    ev.interventions(rl.interventions());

    for (auto & iter : modified_fields) {
        ev.fields()[iter.first] = std::move(iter.second);
    }

    for (auto & iter : modified_traits) {
        ev.traits()[iter.first] = std::move(iter.second);
    }
}

template<typename Cont, typename Prop>
static inline void erase_if(Cont & c, Prop p)
{
    for (auto it = c.begin(); it != c.end();) {
        if (p(*it)) {
            it = c.erase(it);
        } else {
            ++it;
        }
    }
}

static void update_state(std::map<rule_id_t, struct rule_state> & rules_state, const rule & rl, const event & ev)
{
    // reset activation rule
    if (!rl.activation_group().group_name.empty() && rl.activation_group().reset) {
        const auto & iter = rules_state.find(rl.id());
        if (iter != rules_state.end()) {
            std::lock_guard<std::mutex> lg{ iter->second.mutex };
            if (rl.same_field().empty()) {
                iter->second.mevents.clear();
            } else {
                const auto & actual_field = ev.fields().find(rl.same_field());
                if (actual_field != ev.fields().end()) {
                    erase_if(iter->second.mevents, [&rl, &actual_field](const auto & elem) {
                        const auto & stored_field = elem.second.fields().find(rl.same_field());
                        return stored_field != elem.second.fields().end() && actual_field->second == stored_field->second;
                    });
                }
            }
        }
    }
}

static void check_top_rules(event & e, const std::vector<rule> & rules, unsigned short depth, std::map<rule_id_t, struct rule_state> & rules_state,
                            bool verbose)
{
    std::vector<std::tuple<const rule *, std::vector<std::pair<std::string, std::string>>, std::vector<std::pair<std::string, std::string>>>>
      top_matching_rules;

    if (verbose)
        std::cout << "    Processing rules (#" << rules.size() << ") level " << depth << "\n";

    for (auto const & r : rules) {
        if (verbose)
            std::cout << "    Checking level " << depth << " rule " << r.id() << " ...";

        auto result = check_rule(e, r, rules_state, verbose);
        if (std::get<0>(result)) {
            top_matching_rules.emplace_back(&r, std::move(std::get<1>(result)), std::move(std::get<2>(result)));
        }
    }

    if (verbose)
        std::cout << "    Matching level " << depth << " rule: " << top_matching_rules.size() << "\n";

    if (top_matching_rules.size() > 0) {
        priority_t max_priority{ 0 };
        rule_id_t min_id{ static_cast<rule_id_t>(-1) };
        std::vector<std::pair<std::string, std::string>> modified_fields, modified_traits;
        const rule * fit{ nullptr };
        for (auto & ex : top_matching_rules) {
            const rule * rl = std::get<0>(ex);
            if (rl->priority() > max_priority || (rl->priority() == max_priority && rl->id() < min_id)) {
                fit = rl;
                min_id = fit->id();
                max_priority = fit->priority();
                modified_fields = std::move(std::get<1>(ex));
                modified_traits = std::move(std::get<2>(ex));
            }
        }

        if (verbose)
            std::cout << "    Level " << depth << " rule fit: " << fit->id() << "\n";

        update_event(e, *fit, modified_fields, modified_traits);
        update_state(rules_state, *fit, e);

        if (verbose)
            std::cout << "    Checking child rules (#" << fit->children().size() << ") ...\n";

        check_top_rules(e, fit->children(), depth + 1, rules_state, verbose);
    }
}

static void format_log(event & e, const std::vector<format> & formats, bool verbose)
{
    for (const auto & f : formats) {
        std::smatch match;
        if (!std::regex_match(e.logstr(), match, f.reg())) {
            if (verbose)
                std::cout << f.name() << " not matching|";
            continue;
        }

        if (verbose)
            std::cout << f.name() << " matching\n";
        for (size_t i = 1; i < match.size(); ++i) {
            e.fields()[f.fields()[i - 1]] = match[i];
        }

        e.traits()["format"] = f.name();

        return;
    }

    if (verbose)
        std::cout << "no format match\n";

    e.traits()["format"] = "unknown";
}

event process_log(const libs::source_event & se, bool verbose, const rule_cfg & rules, std::map<rule_id_t, struct rule_state> & rules_state)
{
    if (verbose) {
        std::cout << "\n  Processing '" << se.message << "' ...\n";
    }
    event e{ se };

    if (verbose) {
        std::cout << "    Format (#" << rules.formats.size() << ") ...  ";
    }
    format_log(e, rules.formats, verbose);

    if (verbose) {
        std::cout << "      Traits:\n";

        for (const auto & elem : e.traits()) {
            std::cout << "        " << elem.first << " -> " << elem.second << "\n";
        }
        std::cout << "      Extracted fields:\n";
        for (const auto & elem : e.fields()) {
            std::cout << "        " << elem.first << " -> ##" << elem.second << "##\n";
        }

        std::cout << "      Groups: " << e.groups_2_str() << "\n";
        std::cout << "      Priority: " << e.priority() << "\n";
    }

    check_top_rules(e, rules.std_rules, 1, rules_state, verbose);

    if (verbose) {
        std::cout << "      Traits:\n";

        for (const auto & elem : e.traits()) {
            std::cout << "        " << elem.first << " -> ##" << elem.second << "##\n";
        }
        std::cout << "      Extracted fields:\n";
        for (const auto & elem : e.fields()) {
            std::cout << "        " << elem.first << " -> ##" << elem.second << "##\n";
        }

        std::cout << "      Groups: " << e.groups_2_str() << "\n";
        std::cout << "      Priority: " << e.priority() << "\n";
    }

    if (verbose)
        std::cout << "    Checking group rules...\n";

    check_top_rules(e, rules.group_rules, 1, rules_state, verbose);

    if (verbose) {
        std::cout << "      Traits:\n";

        for (const auto & elem : e.traits()) {
            std::cout << "        " << elem.first << " -> ##" << elem.second << "##\n";
        }
        std::cout << "      Extracted fields:\n";
        for (const auto & elem : e.fields()) {
            std::cout << "        " << elem.first << " -> ##" << elem.second << "##\n";
        }

        std::cout << "      Groups: " << e.groups_2_str() << "\n";
        std::cout << "      Priority: " << e.priority() << "\n";

        std::cout << "  End extracting.\n";
    }

    return e;
}

}  // namespace ctguard::research
