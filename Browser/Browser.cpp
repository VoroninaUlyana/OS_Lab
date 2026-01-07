#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <strsafe.h>
#include <limits>
#include "SharedConstants.h"

int main() 
{
    try
    {
    int N, M;
    std::cout << "--- Browser Manager ---\n";
    std::cout << "Enter max simultaneous downloads (N): ";
    if (!(std::cin >> N))
        throw std::runtime_error("Invalid input for N");
    std::cout << "Enter total files in queue (M): ";
    if (!(std::cin >> M))
        throw std::runtime_error("Invalid input for M");
    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
    if (M <= N) 
    {
        std::cerr << "Error: M must be greater than or equal to N.\n";
        return 1;
    }
    if (N <= 0) 
    {
        std::cerr << "Error: N must be a positive integer.\n";
        return 1;
    }
    HANDLE hSemaphore = CreateSemaphore(nullptr, N, N, kSemaphoreName);
    if (nullptr == hSemaphore) 
    {
        throw std::runtime_error("Failed to create semaphore. Error code: " + std::to_string(GetLastError()));
    }
    HANDLE hMutex = CreateMutex(nullptr, FALSE, kMutexName);
    if (nullptr == hMutex) 
    {
        CloseHandle(hSemaphore);
        throw std::runtime_error("Failed to create mutex. Error code: " + std::to_string(GetLastError()));
    }
    HANDLE hExitEvent = CreateEvent(nullptr, TRUE, FALSE, kEventName);
    if (nullptr == hExitEvent) 
    {
        CloseHandle(hSemaphore);
        CloseHandle(hMutex);
        throw std::runtime_error("Failed to create event. Error code: " + std::to_string(GetLastError()));
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
            std::cerr << "Warning: Failed to launch Downloader.exe for " << i + 1 << ". WinAPI Error: " << GetLastError() << std::endl;
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
    }
    catch (const std::exception& e)
    {
        std::cerr << "\nCRITICAL ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}