#include <iostream>
#include <windows.h>

int main()
{
    std::cout << "Hello World!\n";

    // Allocate some memory and write a message to it
    float dynamicMemory2 = 44.0f; // 43 1 byte chars plus the null end terminator
    char* dynamicMemory = new char[44]; // 43 1 byte chars plus the null end terminator
    strcpy_s(dynamicMemory, 44, "This is a test string in the target process");

    // Print the address of the dynamic memory for verification
    std::cout << "Dynamic memory allocated at address: " << (void*)dynamicMemory << std::endl;
   // std::cout << "Dynamic memory allocated at address: " << (void*)dynamicMemory2 << std::endl;

    // Keep the application running to allow injection
    std::cout << "Press Enter to exit...\n";
    std::cin.get();

    std::cout << "Stored string memory: " << dynamicMemory << std::endl;
    std::cout << "Modified integer memory: " << (float)dynamicMemory2 << std::endl;

    // Keep the application running to allow reading output of written memory
    std::cout << "Press Enter to exit...\n";
    std::cin.get();

    delete[] dynamicMemory;

    return 0;
}
