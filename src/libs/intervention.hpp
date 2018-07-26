#pragma once

#include <string>

namespace ctguard::libs {

struct intervention_cmd
{
    std::string name;
    std::string argument;

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive(name, argument);
    }
};

}  // namespace ctguard::libs
