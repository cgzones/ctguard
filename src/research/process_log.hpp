#pragma once

#include <map>
#include <string>

#include "event.hpp"
#include "research.hpp"
#include "rule.hpp"

namespace ctguard ::research {

event process_log(const libs::source_event & se, bool verbose, const rule_cfg & extractors, std::map<rule_id_t, struct rule_state> & rules_state);

}  // namespace ctguard::research
