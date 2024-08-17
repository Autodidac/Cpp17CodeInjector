#include "Obfuscator.h"
#include <fstream>
#include <random>
#include <stdexcept>
#include <iostream>
#include <filesystem>
#include <vector>

std::string Obfuscator::generateRandomName(size_t length) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, sizeof(alphanum) - 2);

    std::string name;
    for (size_t i = 0; i < length; ++i) {
        name += alphanum[distribution(generator)];
    }

    std::cout << "Generated random function sig: " << name << std::endl; // Debug output
    return name;
}

void Obfuscator::generateHeader(const std::string& filePath) {
    std::cout << "Obfuscate called with path: " << filePath << std::endl; // Debug output

    std::string obfuscatedName = generateRandomName();

    // Read the file content
    std::ifstream fileIn(filePath);
    if (!fileIn.is_open()) {
        std::cerr << "Failed to open file for reading: " << filePath << std::endl;
        throw std::runtime_error("Failed to open file for reading: " + filePath);
    }

    std::vector<std::string> lines;
    std::string line;
    bool lineFound = false;

    while (std::getline(fileIn, line)) {
        if (line.find("    void obfuscated::") != std::string::npos) {
            lines.push_back("   void obfuscated::" + obfuscatedName + "();");
            lines.push_back("   void " + obfuscatedName + "(){}");
            lineFound = true;
        }
        else {
            lines.push_back(line);
        }
    }
    fileIn.close();

    // If the line was not found, add it
    if (!lineFound) {
        lines.push_back("namespace obfuscated {");
        lines.push_back("   void obfuscated::" + obfuscatedName + "();");
        lines.push_back("   void " + obfuscatedName + "(){}");
        lines.push_back("}");
    }

    // Write the modified content back to the file
    std::ofstream fileOut(filePath);
    if (!fileOut.is_open()) {
        std::cerr << "Failed to open file for writing: " << filePath << std::endl;
        throw std::runtime_error("Failed to open file for writing: " + filePath);
    }

    for (const auto& l : lines) {
        fileOut << l << "\n";
    }

    fileOut.close();
}
