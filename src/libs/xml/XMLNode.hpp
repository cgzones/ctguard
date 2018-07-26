#pragma once

#include <ostream>
#include <vector>

namespace ctguard::libs::xml {

class XMLNode
{
  public:
    XMLNode(std::string name, std::vector<std::pair<std::string, std::string>> attributes);
    XMLNode(std::string name, std::vector<std::pair<std::string, std::string>> attributes, std::string value, std::vector<XMLNode> children);

    [[nodiscard]] const std::string & name() const noexcept { return m_name; }
    [[nodiscard]] const std::string & value() const noexcept { return m_value; }
    [[nodiscard]] const std::vector<XMLNode> & children() const noexcept { return m_children; }
    [[nodiscard]] const std::vector<std::pair<std::string, std::string>> & attributes() const noexcept { return m_attributes; }

    friend std::ostream & operator<<(std::ostream & out, const XMLNode & node);

  private:
    std::string m_name;
    std::vector<std::pair<std::string, std::string>> m_attributes;
    std::string m_value;
    std::vector<XMLNode> m_children;
};

}  // namespace ctguard::libs::xml
