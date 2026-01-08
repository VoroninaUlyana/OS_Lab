#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>

void RunParentMode(const char* exePath);
void RunChildMode();

int main(int argc, char* argv[]) 
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
    return 0;
}

void RunParentMode(const char* exePath) 
{
    HANDLE hPipeParentToChildRead = NULL, hPipeParentToChildWrite = NULL;
    HANDLE hPipeChildToParentRead = NULL, hPipeChildToParentWrite = NULL;
    SECURITY_ATTRIBUTES saAttr{};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
    if (FALSE == CreatePipe(&hPipeParentToChildRead, &hPipeParentToChildWrite, &saAttr, 0)) 
    {
        std::cerr << "[Parent] Error: Pipe 1 creation failed.\n";
        return;
    }
    if (FALSE == CreatePipe(&hPipeChildToParentRead, &hPipeChildToParentWrite, &saAttr, 0)) 
    {
        std::cerr << "[Parent] Error: Pipe 2 creation failed.\n";
        CloseHandle(hPipeParentToChildRead);
        CloseHandle(hPipeParentToChildWrite);
        return;
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
        NULL, &command[0],
        NULL, NULL, TRUE,
        0, NULL, NULL,
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
        std::cerr << "[Parent] Invalid input size." << std::endl;
    }
    else
    {
        std::vector<int> arr(static_cast<size_t>(n));
        std::cout << "[Parent] Enter " << n << " elements:" << std::endl;
        for (int i = 0; i < n; ++i) 
        {
            std::cin >> arr[static_cast<size_t>(i)];
        }
        DWORD written;
        WriteFile(hPipeParentToChildWrite, &n, sizeof(int), &written, NULL);
        WriteFile(hPipeParentToChildWrite, arr.data(), static_cast<DWORD>(n * sizeof(int)), &written, NULL);
        std::cout << "[Parent] Data sent to child process.\n";
        CloseHandle(hPipeParentToChildWrite);
        hPipeParentToChildWrite = NULL;
        int evenCount = 0;
        DWORD readBytes;
        if (ReadFile(hPipeChildToParentRead, &evenCount, sizeof(int), &readBytes, NULL))
        {
            std::cout << "[Parent] Received result from child process.\n";
            std::cout << "Number of even elements: " << evenCount << "\n";
        }
        else
        {
            std::cerr << "[Parent] Error reading from pipe.\n";
        }
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (NULL != hPipeParentToChildWrite)
    {
        CloseHandle(hPipeParentToChildWrite);
    }
    CloseHandle(hPipeChildToParentRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    std::cout << "[Parent] Process finished." << std::endl;
}

void RunChildMode() 
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    int n = 0;
    DWORD bytesRead;
    if (FALSE == ReadFile(hStdin, &n, sizeof(int), &bytesRead, NULL) || bytesRead == 0) 
    {
        std::cerr << "[Child] Error reading array size.\n";
        return;
    }
    std::vector<int> arr(static_cast<size_t>(n));
    if (FALSE == ReadFile(hStdin, arr.data(), static_cast<DWORD>(n * sizeof(int)), &bytesRead, NULL))
    {
        std::cerr << "[Child] Error reading array data.\n";
        return;
    }
    std::cout << "[Child] Array received. Processing data...\n";
    int evenCount = 0;
    for (const auto& value : arr)
    {
        if (0 == value % 2)
        {
            evenCount++;
        }
    }
    std::cout << "[Child] Result calculated. Sending to parent...\n";
    DWORD bytesWritten;
    WriteFile(hStdout, &evenCount, sizeof(int), &bytesWritten, NULL);
    std::cout << "[Child] Terminate the child process.\n";
}