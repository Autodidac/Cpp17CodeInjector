#include <iostream>
#include <string>
#include "Obfuscator.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <output_header_path>" << std::endl;
        return 1;
    }

    std::string outputHeaderPath = argv[1];

    std::cout << "Obfuscator started" << std::endl;
    std::cout << "Output header path: " << outputHeaderPath << std::endl;

    try {
        Obfuscator obfuscator;
        obfuscator.generateHeader(outputHeaderPath);
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Program ended successfully" << std::endl;

    return 0;
}
