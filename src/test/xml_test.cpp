#include <fstream>
#include <iostream>
#include <sstream>
#include <string.h>

#include "../libs/xml/XMLNode.hpp"
#include "../libs/xml/XMLparser.hpp"

using namespace ctguard::libs::xml;

int main(int argc, char ** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " xml_file\n";
        return EXIT_FAILURE;
    }

    std::ifstream xml_input{ argv[1] };

    if (!xml_input.is_open()) {
        std::cerr << "Can not open '" << argv[1] << "': " << ::strerror(errno) << "\n";
        return EXIT_FAILURE;
    }

    try {
        XMLparser parser{ xml_input };

        std::cout << "parsing " << argv[1] << "...";

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
