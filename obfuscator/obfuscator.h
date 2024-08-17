#pragma once

#include <string>

class Obfuscator {
public:
    void generateHeader(const std::string& filePath);

private:
    std::string generateRandomName(size_t length = 10);
};

