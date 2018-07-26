#include "watch.hpp"

namespace ctguard::diskscan {

std::ostream & operator<<(std::ostream & os, watch::scan_option so)
{
    switch (so) {
        case watch::scan_option::FULL:
            os << "FULL";
            break;
    }

    return os;
}

}  // namespace ctguard::diskscan
