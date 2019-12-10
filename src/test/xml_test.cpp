#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "../libs/xml/XMLNode.hpp"
#include "../libs/xml/XMLparser.hpp"

using ctguard::libs::xml::XMLNode;
using ctguard::libs::xml::XMLparser;

int main(int argc, char ** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " xml_file\n";  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return EXIT_FAILURE;
    }

    std::ifstream xml_input{ argv[1] };  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    if (!xml_input.is_open()) {
        std::cerr << "Can not open '" << argv[1] << "': " << ::strerror(errno) << "\n";  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return EXIT_FAILURE;
    }

    try {
        XMLparser parser{ xml_input };

        std::cout << "parsing " << argv[1] << "...";  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        XMLNode root{ parser.parse() };

        std::cout << " done\n";

        std::cout << "\nxml output:\n";

        std::cout << root;

        std::cout << "\n\nFinished\n";

    } catch (const std::exception & e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
