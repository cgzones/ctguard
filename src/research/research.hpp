#pragma once

#include "rule.hpp"
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>

namespace ctguard::research {

inline bool UNIT_TEST{ false };
inline volatile std::atomic<bool> RUNNING{ true };

struct rule_state
{
    std::mutex mutex;
    std::multimap<std::time_t, event> mevents;
    std::time_t unless_triggered{ 0 };
    short unsigned unless_timeout{ 0 };
    event unless_event;
};

}  // namespace ctguard::research
