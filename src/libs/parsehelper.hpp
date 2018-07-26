#pragma once

#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace ctguard::libs {

[[nodiscard]] extern inline bool parse_bool(std::string_view value)
{
    if (value == "true" || value == "TRUE") {
        return true;
    }
    if (value == "false" || value == "FALSE") {
        return false;
    }

    throw std::out_of_range{ "Invalid bool value" };
}

template<typename T>
[[nodiscard]] extern T parse_integral(const std::string & content)
{
    std::size_t pos;
    unsigned long result_orig;
    try {
        result_orig = std::stoul(content, &pos);
    } catch (const std::exception & e) {
        throw std::out_of_range{ std::string("parse error: ") + e.what() };
    }
    if (pos != content.length()) {
        throw std::out_of_range{ "incomplete parse (" + std::to_string(pos) + '/' + std::to_string(content.length()) + ')' };
    }
    if (result_orig > std::numeric_limits<T>::max()) {
        throw std::out_of_range{ "value out of range (" + std::to_string(std::numeric_limits<T>::max()) + ')' };
    }
    return static_cast<T>(result_orig);
}

[[nodiscard]] extern inline unsigned int parse_second_duration(const std::vector<std::string> & options)
{
    if (options.size() == 1) {
        return parse_integral<unsigned int>(options[0]);
    }

    if (options.size() != 2) {
        throw std::out_of_range{ "can not parse duration from " + std::to_string(options.size()) + " arguments" };
    }

    const unsigned int number = parse_integral<unsigned int>(options[0]);

    if (options[1] == "s") {
        return number;
    } else if (options[1] == "m") {
        return number * 60;
    } else if (options[1] == "h") {
        return number * 60 * 60;
    } else {
        throw std::out_of_range{ "unknown timeunit '" + options[1] + "'" };
    }
}

}  // namespace ctguard::libs
