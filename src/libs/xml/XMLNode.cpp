#include "XMLNode.hpp"

namespace ctguard::libs::xml {

XMLNode::XMLNode(std::string name, std::vector<std::pair<std::string, std::string>> attributes)
  : m_name{ std::move(name) }, m_attributes{ std::move(attributes) }
{}

XMLNode::XMLNode(std::string name, std::vector<std::pair<std::string, std::string>> attributes, std::string value, std::vector<XMLNode> children)
  : m_name{ std::move(name) }, m_attributes{ std::move(attributes) }, m_value{ std::move(value) }, m_children{ std::move(children) }
{}

std::ostream & operator<<(std::ostream & out, const XMLNode & node)
{
    out << '<' << node.m_name;

    for (const auto & attr : node.m_attributes) {
        out << " " << attr.first << "=\"" << attr.second << "\"";
    }

    out << '>';

    if (!node.m_value.empty()) {
        out << node.m_value;
    }

    for (const XMLNode & n : node.m_children) {
        out << n;
    }

    out << "</" << node.m_name << ">";

    return out;
}

} /* namespace ctguard::libs::xml */
