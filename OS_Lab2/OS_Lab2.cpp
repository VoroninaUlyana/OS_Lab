#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
using namespace std;

void parent_mode(const char* exePath);
void child_mode();

int main(int argc, char* argv[]) 
{
    if (argc > 1 && string(argv[1]) == "child") 
    {
        cout << "[Child] Started in child mode.\n";
        child_mode();
    }
    else 
    {
        cout << "[Parent] Started in parent mode.\n";
        parent_mode(argv[0]);
    }
    return 0;
}

void parent_mode(const char* exePath) 
{
    HANDLE hPipeParentToChildRead, hPipeParentToChildWrite;
    HANDLE hPipeChildToParentRead, hPipeChildToParentWrite;
    SECURITY_ATTRIBUTES saAttr{ sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    CreatePipe(&hPipeParentToChildRead, &hPipeParentToChildWrite, &saAttr, 0);
    CreatePipe(&hPipeChildToParentRead, &hPipeChildToParentWrite, &saAttr, 0);

    SetHandleInformation(hPipeParentToChildWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hPipeChildToParentRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdInput = hPipeParentToChildRead;
    si.hStdOutput = hPipeChildToParentWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    ostringstream oss;
    oss << "\"" << exePath << "\" child";
    string command = oss.str();

    cout << "[Parent] Creating child process...\n";

    BOOL success = CreateProcessA(
        NULL, &command[0], NULL, NULL, TRUE,
        0, NULL, NULL, &si, &pi
    );

    if (!success) 
    {
        cerr << "[Parent] Failed to create child process.\n";
        return;
    }

    CloseHandle(hPipeParentToChildRead);
    CloseHandle(hPipeChildToParentWrite);

    int n;
    cout << "[Parent] Enter array size: ";
    cin >> n;

    vector<int> arr(n);
    cout << "[Parent] Enter array elements:\n";
    for (int i = 0; i < n; ++i)
        cin >> arr[i];

    DWORD written;
    WriteFile(hPipeParentToChildWrite, &n, sizeof(int), &written, NULL);
    WriteFile(hPipeParentToChildWrite, arr.data(), n * sizeof(int), &written, NULL);
    CloseHandle(hPipeParentToChildWrite);

    int result;
    DWORD bytesRead;
    ReadFile(hPipeChildToParentRead, &result, sizeof(int), &bytesRead, NULL);
    cout << "[Parent] Result received from child: " << result << "\n";

    CloseHandle(hPipeChildToParentRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void child_mode() 
{
    cout << "[Child] Child mode placeholder.\n";
}
