#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <tchar.h>
#include <strsafe.h>
#include <limits>
#include "SharedConstants.h"

int main() 
{
    int N, M;
    std::cout << "--- Browser Manager ---\n";
    std::cout << "Enter max simultaneous downloads (N): ";
    if (!(std::cin >> N))
        return 0;
    std::cout << "Enter total files in queue (M): ";
    if (!(std::cin >> M))
        return 0;
    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
    if (M <= N) 
    {
        std::cout << "Note: M should be greater than N.\n";
    }
    if (N <= 0) 
    {
        std::cout << "Note: N should be greater than 0.\n";
    }
    HANDLE hSemaphore = CreateSemaphore(nullptr, N, N, kSemaphoreName);
    HANDLE hMutex = CreateMutex(nullptr, FALSE, kMutexName);
    HANDLE hExitEvent = CreateEvent(nullptr, TRUE, FALSE, kEventName);
    if (nullptr == hSemaphore || nullptr == hMutex || nullptr == hExitEvent)
    {
        std::cerr << "Error: Could not create kernel objects.\n";
        return 1;
    }
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    std::vector<HANDLE> childProcesses;
    std::cout << "Starting download processes...\n";
    for (int i = 0; i < M; ++i) 
    {
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        TCHAR cmdLine[MAX_PATH];
        StringCchPrintf(cmdLine, MAX_PATH, _T("Downloader.exe file_%d.dat"), i + 1);
        if (CreateProcess(nullptr, cmdLine, nullptr, nullptr, FALSE,0, nullptr, nullptr, &si, &pi))
        {
            childProcesses.push_back(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        else 
        {
            std::cerr << "Error: Failed to launch Downloader.exe\n";
        }
    }
    std::cout << "\n=============================================\n";
    std::cout << "Browser is running.\n";
    std::cout << "Active downloads: " << N << ", In queue: " << (M - N) << "\n";
    std::cout << "PRESS [ENTER] TO STOP THE BROWSER...\n";
    std::cout << "=============================================\n";
    std::cin.get();
    WaitForSingleObject(hMutex, INFINITE);
    std::cout << "\n[!] Signal sent! Closing browser...\n";
    std::cout << "[!] Waiting for child processes to finish (max 4 seconds)...\n";
    ReleaseMutex(hMutex);
    SetEvent(hExitEvent);
    const int MAX_WAIT = MAXIMUM_WAIT_OBJECTS;
    for (size_t i = 0; i < childProcesses.size(); i += MAX_WAIT) 
    {
        int count = min((int)(childProcesses.size() - i), MAX_WAIT);
        WaitForMultipleObjects(count, &childProcesses[i], TRUE, INFINITE);
    }
    std::cout << "All processes closed. Press Enter to exit.\n";
    for (HANDLE h : childProcesses) CloseHandle(h);
    CloseHandle(hSemaphore);
    CloseHandle(hMutex);
    CloseHandle(hExitEvent);
    std::cin.get();
    return 0;
}