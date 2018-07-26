#include "event.hpp"

#include "rule.hpp"

#include <chrono>
#include <iomanip>
#include <regex>

namespace ctguard::research {

event::event(const libs::source_event & se) : m_logstr{ se.message }, m_control_message{ se.control_message }
{
    m_traits.insert({ "hostname", se.hostname });
    m_traits.insert({ "source_program", se.source_program });
    m_traits.insert({ "source_domain", se.source_domain });
    m_traits.insert({ "control", [&]() {
                         if (m_control_message)
                             return "true";
                         else
                             return "false";
                     }() });

    {
        struct tm ts;
        ::localtime_r(&se.time_scanned, &ts);
        std::ostringstream oss;
        oss << std::put_time(&ts, "%c %Z");
        m_traits.insert({ "time_scanned", oss.str() });
    }
    {
        struct tm ts;
        ::localtime_r(&se.time_send, &ts);
        std::ostringstream oss;
        oss << std::put_time(&ts, "%c %Z");
        m_traits.insert({ "time_send", oss.str() });
    }
}

void event::add_groups(const std::set<std::string> & input)
{
    m_groups.insert(input.begin(), input.end());
}

std::string event::groups_2_str() const
{
    std::ostringstream sso;
    for (const std::string & s : m_groups) {
        sso << s << ' ';
    }

    return sso.str();
}

event event::update(const rule & rule) const
{
    event e{ *this };
    e.m_priority = rule.priority();
    e.m_rule_id = rule.id();
    e.m_description = rule.description();
    e.add_groups(rule.groups());
    return e;
}

}  // namespace ctguard::research
