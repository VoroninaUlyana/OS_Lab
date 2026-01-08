#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <limits>

static const int EVEN_DIVISOR = 2;

int CountEvenElements(const std::vector<int>& arr)
{
    int evenCount = 0;
    for (const auto& value : arr)
    {
        if (0 == value % EVEN_DIVISOR)
        {
            evenCount++;
        }
    }
    return evenCount;
}

void ReportError(const std::string& message) 
{
    DWORD errorCode = GetLastError();
    std::cerr << message << " (Windows Error Code: " << errorCode << ")" << std::endl;
    throw std::runtime_error(message);
}

void RunParentMode(const char* exePath);
void RunChildMode();

int main(int argc, char* argv[]) 
{
    try
    {
        if (argc > 1 && std::string(argv[1]) == "child")
        {
            std::cout << "[Child] Starting child process...\n";
            RunChildMode();
        }
        else
        {
            std::cout << "[Parent] Starting parent process...\n";
            RunParentMode(argv[0]);
        }
    }
    catch (const std::exception& e) 
    {
        std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

void RunParentMode(const char* exePath) 
{
    HANDLE hPipeParentToChildRead = nullptr, hPipeParentToChildWrite = nullptr;
    HANDLE hPipeChildToParentRead = nullptr, hPipeChildToParentWrite = nullptr;
    SECURITY_ATTRIBUTES saAttr{};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;
    if (FALSE == CreatePipe(&hPipeParentToChildRead, &hPipeParentToChildWrite, &saAttr, 0)) 
    {
        ReportError("Failed to create Pipe 1");
    }
    if (FALSE == CreatePipe(&hPipeChildToParentRead, &hPipeChildToParentWrite, &saAttr, 0)) 
    {
        CloseHandle(hPipeParentToChildRead);
        CloseHandle(hPipeParentToChildWrite);
        ReportError("Failed to create Pipe 2");
    }
    SetHandleInformation(hPipeParentToChildWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hPipeChildToParentRead, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdInput = hPipeParentToChildRead;
    si.hStdOutput = hPipeChildToParentWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;
    std::ostringstream oss;
    oss << "\"" << exePath << "\" child";
    std::string command = oss.str();
    std::cout << "[Parent] Creating child process...\n";
    BOOL success = CreateProcessA(
        nullptr, &command[0],
        nullptr, nullptr, TRUE,
        0, nullptr, nullptr,
        &si, &pi
    );
    if (FALSE == success)
    {
        std::cerr << "[Parent] Error: Child process creation failed.\n";
        CloseHandle(hPipeParentToChildRead); CloseHandle(hPipeParentToChildWrite);
        CloseHandle(hPipeChildToParentRead); CloseHandle(hPipeChildToParentWrite);
        return;
    }
    CloseHandle(hPipeParentToChildRead);
    CloseHandle(hPipeChildToParentWrite);
    std::cout << "[Parent] Child process created successfully.\n";
    int n = 0;
    std::cout << "[Parent] Enter array size: ";
    if (!(std::cin >> n) || n <= 0) 
    {
        throw std::invalid_argument("Array size must be a positive integer");
    }
    else
    {
        std::vector<int> arr(static_cast<size_t>(n));
        std::cout << "[Parent] Enter " << n << " elements:" << std::endl;
        for (int i = 0; i < n; ++i) 
        {
            if (!(std::cin >> arr[static_cast<size_t>(i)]))
            {
                throw std::runtime_error("Invalid array element input");
            }
        }
        std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
        DWORD written;
        if (FALSE == WriteFile(hPipeParentToChildWrite, &n, sizeof(int), &written, nullptr))
        {
            ReportError("Failed to write size to pipe");
        }
        if (FALSE == WriteFile(hPipeParentToChildWrite, arr.data(), static_cast<DWORD>(n * sizeof(int)), &written, nullptr))
        {
            ReportError("Failed to write data to pipe");
        }
        std::cout << "[Parent] Data sent to child process.\n";
        CloseHandle(hPipeParentToChildWrite);
        hPipeParentToChildWrite = nullptr;
        int evenCount = 0;
        DWORD readBytes;
        if (ReadFile(hPipeChildToParentRead, &evenCount, sizeof(int), &readBytes, nullptr))
        {
            std::cout << "[Parent] Received result from child process.\n";
            std::cout << "Number of even elements: " << evenCount << "\n";
        }
        else
        {
            ReportError("Failed to read result from child");
        }
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (nullptr != hPipeParentToChildWrite)
    {
        CloseHandle(hPipeParentToChildWrite);
    }
    CloseHandle(hPipeChildToParentRead);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    std::cout << "[Parent] Process finished." << std::endl;
}

void RunChildMode() 
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    int n = 0;
    DWORD bytesRead;
    if (FALSE == ReadFile(hStdin, &n, sizeof(int), &bytesRead, nullptr) || bytesRead == 0)
    {
        std::cerr << "[Child] Error reading array size.\n";
        return;
    }
    std::vector<int> arr(static_cast<size_t>(n));
    if (FALSE == ReadFile(hStdin, arr.data(), static_cast<DWORD>(n * sizeof(int)), &bytesRead, nullptr))
    {
        std::cerr << "[Child] Error reading array data.\n";
        return;
    }
    std::cout << "[Child] Array received. Processing data...\n";
    int evenCount = CountEvenElements(arr);
    std::cout << "[Child] Result calculated. Sending to parent...\n";
    DWORD bytesWritten;
    WriteFile(hStdout, &evenCount, sizeof(int), &bytesWritten, nullptr);
    std::cout << "[Child] Terminate the child process.\n";
}