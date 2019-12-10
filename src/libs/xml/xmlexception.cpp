#include "xmlexception.hpp"

namespace ctguard::libs::xml {

xml_exception::xml_exception(std::string_view msg) : m_msg{ msg } {}

const char * xml_exception::what() const noexcept
{
    return m_msg.c_str();
}

} /* namespace ctguard::libs::xml */
