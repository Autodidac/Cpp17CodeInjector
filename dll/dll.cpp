#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <regex>

// Function to get the process ID of the target process
DWORD GetProcessByName(const wchar_t* lpProcessName)
{
    PROCESSENTRY32 pe32 = { 0 };
    pe32.dwSize = sizeof(pe32);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return -1;

    if (!Process32First(hSnapshot, &pe32))
    {
        CloseHandle(hSnapshot);
        return -1;
    }

    do
    {
        if (wcscmp(pe32.szExeFile, lpProcessName) == 0)
        {
            CloseHandle(hSnapshot);
            return pe32.th32ProcessID;
        }
    } while (Process32Next(hSnapshot, &pe32));

    CloseHandle(hSnapshot);
    return -1;
}

// Function to extract directory path from full path
std::string ExtractDirectoryPath(const std::string& fullPath)
{
    size_t pos = fullPath.find_last_of("\\/");
    if (pos != std::string::npos)
        return fullPath.substr(0, pos);
    return fullPath;
}

// Function to convert wide string to narrow string
std::string WStringToString(const std::wstring& wstr)
{
    if (wstr.empty())
        return "";

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &str[0], size_needed, NULL, NULL);
    return str;
}

// Example C++ code to extract hexadecimal-like patterns
std::vector<std::string> extractHexAddresses(const std::string& input) {
    std::vector<std::string> addresses;
    std::regex hexPattern("0x[0-9a-fA-F]+");
    std::smatch match;

    std::string::const_iterator searchStart(input.cbegin());
    while (std::regex_search(searchStart, input.cend(), match, hexPattern)) {
        addresses.push_back(match[0]);
        searchStart = match.suffix().first;
    }

    return addresses;
}

std::string filterPrintable(const std::string& str) {
    std::string result;
    for (char c : str) {
        if (std::isprint(static_cast<unsigned char>(c)) || c == ' ') {
            result += c;
        }
    }
    return result;
}

void ScanMemoryForStringsAndAddresses(HANDLE hProcess, uintptr_t baseAddress, SIZE_T regionSize, std::ofstream& outfile)
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    uintptr_t startAddress = baseAddress;
    uintptr_t endAddress = startAddress + regionSize;

    for (uintptr_t addr = startAddress; addr < endAddress; addr += sysInfo.dwPageSize)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQueryEx(hProcess, (LPCVOID)addr, &mbi, sizeof(mbi)))
            continue;

        if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
        {
            std::vector<uint8_t> buffer(mbi.RegionSize);
            SIZE_T bytesRead;
            if (ReadProcessMemory(hProcess, mbi.BaseAddress, buffer.data(), mbi.RegionSize, &bytesRead))
            {
                // Detect null-terminated strings
                size_t start = 0;
                while (start < bytesRead)
                {
                    size_t end = start;
                    while (end < bytesRead && buffer[end] != '\0')
                        ++end;

                    if (end > start)
                    {
                        std::string foundString(buffer.begin() + start, buffer.begin() + end);
                        foundString = filterPrintable(foundString);
                        if (!foundString.empty())
                        {
                            outfile << "String: " << foundString << " at address: 0x" << std::hex << (addr + start) << std::dec << std::endl;
                        }
                    }
                    start = end + 1; // Move past the null terminator
                }
            }
        }
    }
}

uintptr_t PatternScan(HANDLE hProcess, uintptr_t baseAddress, SIZE_T regionSize, const std::vector<uint8_t>& pattern)
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    uintptr_t startAddress = baseAddress;
    uintptr_t endAddress = startAddress + regionSize;

    for (uintptr_t addr = startAddress; addr < endAddress; addr += sysInfo.dwPageSize)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQueryEx(hProcess, (LPCVOID)addr, &mbi, sizeof(mbi)))
            continue;

        if (mbi.Protect & PAGE_READONLY || mbi.Protect & PAGE_READWRITE || mbi.Protect & PAGE_EXECUTE_READ)
        {
            std::vector<uint8_t> buffer(mbi.RegionSize);
            SIZE_T bytesRead;
            if (ReadProcessMemory(hProcess, mbi.BaseAddress, buffer.data(), mbi.RegionSize, &bytesRead))
            {
                for (size_t i = 0; i < bytesRead - pattern.size(); ++i)
                {
                    if (std::equal(pattern.begin(), pattern.end(), buffer.begin() + i))
                    {
                        return addr + i;
                    }
                }
            }
        }
    }
    return 0;
}

// Function to scan for an integer value in the process memory
uintptr_t ScanForInt(HANDLE hProcess, int value)
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    uintptr_t startAddress = (uintptr_t)sysInfo.lpMinimumApplicationAddress;
    uintptr_t endAddress = (uintptr_t)sysInfo.lpMaximumApplicationAddress;

    for (uintptr_t addr = startAddress; addr < endAddress;)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQueryEx(hProcess, (LPCVOID)addr, &mbi, sizeof(mbi)))
        {
            addr += sysInfo.dwPageSize;
            continue;
        }

        if (mbi.State == MEM_COMMIT &&
            (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
        {
            std::vector<uint8_t> buffer(mbi.RegionSize);
            SIZE_T bytesRead;
            if (ReadProcessMemory(hProcess, mbi.BaseAddress, buffer.data(), mbi.RegionSize, &bytesRead))
            {
                for (size_t i = 0; i < bytesRead - sizeof(value); ++i)
                {
                    if (memcmp(buffer.data() + i, &value, sizeof(value)) == 0)
                    {
                        return (uintptr_t)mbi.BaseAddress + i;
                    }
                }
            }
        }
        addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    }
    return 0;
}

// Function to scan for a float value in the process memory
uintptr_t ScanForFloat(HANDLE hProcess, float value)
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    uintptr_t startAddress = (uintptr_t)sysInfo.lpMinimumApplicationAddress;
    uintptr_t endAddress = (uintptr_t)sysInfo.lpMaximumApplicationAddress;

    for (uintptr_t addr = startAddress; addr < endAddress;)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQueryEx(hProcess, (LPCVOID)addr, &mbi, sizeof(mbi)))
        {
            addr += sysInfo.dwPageSize;
            continue;
        }

        if (mbi.State == MEM_COMMIT &&
            (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
        {
            std::vector<uint8_t> buffer(mbi.RegionSize);
            SIZE_T bytesRead;
            if (ReadProcessMemory(hProcess, mbi.BaseAddress, buffer.data(), mbi.RegionSize, &bytesRead))
            {
                for (size_t i = 0; i < bytesRead - sizeof(value); ++i)
                {
                    if (memcmp(buffer.data() + i, &value, sizeof(value)) == 0)
                    {
                        return (uintptr_t)mbi.BaseAddress + i;
                    }
                }
            }
        }
        addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    }
    return 0;
}


// Entry point for DLL
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        WCHAR path[MAX_PATH];
        if (GetModuleFileNameW(NULL, path, MAX_PATH) == 0)
        {
            MessageBoxA(NULL, "Failed to get module file name.", "Error", MB_OK | MB_ICONERROR);
            return FALSE;
        }
        std::wstring wpath(path);
        std::string filepath = WStringToString(wpath);

        std::string dirPath = ExtractDirectoryPath(filepath);

        std::string output_file_path = dirPath + "\\strings_and_addresses.txt";

        std::cout << "Output file path: " << output_file_path << std::endl;

        std::ofstream outfile(output_file_path, std::ios::out | std::ios::binary);
        if (!outfile)
        {
            MessageBoxA(NULL, ("Failed to open output file for writing: " + output_file_path).c_str(), "Error", MB_OK | MB_ICONERROR);
            return FALSE;
        }

        DWORD pid = GetProcessByName(L"target.exe");
        if (pid == -1)
        {
            outfile << "Failed to find process 'target.exe'." << std::endl;
            outfile.close();
            return FALSE;
        }

        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!hProcess)
        {
            outfile << "Failed to open process." << std::endl;
            outfile.close();
            return FALSE;
        }

        // Adjust base address and region size according to the target process
        uintptr_t baseAddress = 0x10000;
        SIZE_T regionSize = 0x7FFFFFFF;

        // Scan for strings and addresses
        ScanMemoryForStringsAndAddresses(hProcess, baseAddress, regionSize, outfile);
        /*
        // Example pattern
        std::vector<uint8_t> pattern = { 0x90, 0x90, 0x90 };
        uintptr_t address = PatternScan(hProcess, baseAddress, regionSize, pattern);

        if (address)
        {
            outfile << "Pattern found at address: 0x" << std::hex << address << std::dec << std::endl;
        }
        else
        {
            outfile << "Pattern not found." << std::endl;
        }*/

        /*
        // Integer value to search for
        int targetValue = 44;
        // New value to set
        int newValue = 42;

        uintptr_t address = ScanForInt(hProcess, targetValue);
        if (address)
        {
            std::cout << "Integer found at address: 0x" << std::hex << address << std::dec << std::endl;

            // Modify the integer value
            if (WriteProcessMemory(hProcess, (LPVOID)address, &newValue, sizeof(newValue), NULL))
            {
                std::cout << "Integer value changed successfully." << std::endl;
            }
            else
            {
                std::cerr << "Failed to change integer value." << std::endl;
            }
        }
        else
        {
            std::cerr << "Integer not found." << std::endl;
        }
*/


        // Float value to search for
        float targetFloatValue = 44.0f;
        // New float value to set
        float newFloatValue = 99.9f;

        uintptr_t floatAddress = ScanForFloat(hProcess, targetFloatValue);
        if (floatAddress)
        {
            std::cout << "Float found at address: 0x" << std::hex << floatAddress << std::dec << std::endl;

            // Modify the float value
            if (WriteProcessMemory(hProcess, (LPVOID)floatAddress, &newFloatValue, sizeof(newFloatValue), NULL))
            {
                std::cout << "Float value changed successfully." << std::endl;
            }
            else
            {
                std::cerr << "Failed to change float value." << std::endl;
            }
        }
        else
        {
            std::cerr << "Float not found." << std::endl;
        }

        outfile.close();
        CloseHandle(hProcess);
    }
    return TRUE;
}
