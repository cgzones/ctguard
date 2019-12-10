#pragma once

#include "config.hpp"
#include "rule.hpp"

namespace ctguard::research {

void daemon(const research_config & cfg, const rule_cfg & rules, std::ostream & output);

} /* namespace ctguard::research */
