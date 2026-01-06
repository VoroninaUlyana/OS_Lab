#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <tchar.h>
#include <strsafe.h>
#include <limits>
using namespace std;

const TCHAR* SEMAPHORE_NAME = _T("DownloadSlots");
const TCHAR* MUTEX_NAME = _T("LogAccessMutex");
const TCHAR* EVENT_NAME = _T("BrowserClosingEvent");

int main() 
{
    int N, M;
    cout << "--- Browser Manager ---\n";
    cout << "Enter max simultaneous downloads (N): ";
    if (!(cin >> N)) 
        return 0;
    cout << "Enter total files in queue (M): ";
    if (!(cin >> M)) 
        return 0;
    cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
    if (M <= N) 
    {
        cout << "Note: M should be greater than N.\n";
    }
    if (N <= 0) 
    {
        cout << "Note: N should be greater than 0.\n";
    }
    HANDLE hSemaphore = CreateSemaphore(NULL, N, N, SEMAPHORE_NAME);
    HANDLE hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);
    HANDLE hExitEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_NAME);
    if (!hSemaphore || !hMutex || !hExitEvent) 
    {
        cerr << "Error: Could not create kernel objects.\n";
        return 1;
    }
    vector<HANDLE> childProcesses;
    cout << "Starting download processes...\n";
    for (int i = 0; i < M; ++i) 
    {
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        TCHAR cmdLine[MAX_PATH];
        StringCchPrintf(cmdLine, MAX_PATH, _T("Downloader.exe file_%d.dat"), i + 1);
        if (CreateProcess(NULL, cmdLine, NULL, NULL, FALSE,0, NULL, NULL, &si, &pi)) 
        {
            childProcesses.push_back(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        else 
        {
            cerr << "Error: Failed to launch Downloader.exe\n";
        }
    }
    cout << "\n=============================================\n";
    cout << "Browser is running.\n";
    cout << "Active downloads: " << N << ", In queue: " << (M - N) << "\n";
    cout << "PRESS [ENTER] TO STOP THE BROWSER...\n";
    cout << "=============================================\n";
    cin.get();
    WaitForSingleObject(hMutex, INFINITE);
    cout << "\n[!] Signal sent! Closing browser...\n";
    cout << "[!] Waiting for child processes to finish (max 4 seconds)...\n";
    ReleaseMutex(hMutex);
    SetEvent(hExitEvent);
    const int MAX_WAIT = MAXIMUM_WAIT_OBJECTS;
    for (size_t i = 0; i < childProcesses.size(); i += MAX_WAIT) 
    {
        int count = min((int)(childProcesses.size() - i), MAX_WAIT);
        WaitForMultipleObjects(count, &childProcesses[i], TRUE, INFINITE);
    }
    cout << "All processes closed. Press Enter to exit.\n";
    for (HANDLE h : childProcesses) CloseHandle(h);
    CloseHandle(hSemaphore);
    CloseHandle(hMutex);
    CloseHandle(hExitEvent);
    cin.get();
    return 0;
}