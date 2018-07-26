#pragma once

#include "config.hpp"

#include <iostream>

namespace ctguard::diskscan {

void daemon(const diskscan_config & cfg, bool singlerun, bool unit_test);
}
