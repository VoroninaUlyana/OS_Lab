#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <tchar.h>
#include <ctime>
#include "SharedConstants.h"

struct BracketResult 
{
    int openCount;
    int closeCount;
};

BracketResult PerformTaskLogic() 
{
    std::string content;
    for (int i = 0; i < kMockDataSize; ++i) 
    {
        content += "Data block with (some) content [id=" + std::to_string(i) + "] {check}. ";
    }
    int openBrackets = 0;
    int closeBrackets = 0;
    for (char c : content) 
    {
        if (c == '(' || c == '[' || c == '{') openBrackets++;
        if (c == ')' || c == ']' || c == '}') closeBrackets++;
    }
    return { openBrackets, closeBrackets };
}

int main(int argc, char* argv[]) 
{
    srand(GetTickCount() ^ GetCurrentProcessId());
    DWORD pid = GetCurrentProcessId();
    std::string fileName = "unknown_file.dat";
    if (argc > 1) 
    {
        fileName = argv[1];
    }
    HANDLE hSemaphore = OpenSemaphore(SYNCHRONIZE | SEMAPHORE_MODIFY_STATE, FALSE, kSemaphoreName);
    HANDLE hMutex = OpenMutex(SYNCHRONIZE, FALSE, kMutexName);
    HANDLE hExitEvent = OpenEvent(SYNCHRONIZE, FALSE, kEventName);
    if (nullptr == hSemaphore || nullptr == hMutex || nullptr == hExitEvent)
    {
        return 1;
    }
    HANDLE waitHandles[2];
    waitHandles[0] = hExitEvent;
    waitHandles[1] = hSemaphore;
    DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
    if (waitResult == WAIT_OBJECT_0) 
    {
        WaitForSingleObject(hMutex, INFINITE);
        std::cout << "[PID: " << pid << "] Download interrupted by browser closing.\n";
        ReleaseMutex(hMutex);
    }
    else if (waitResult == WAIT_OBJECT_0 + 1) 
    {
        WaitForSingleObject(hMutex, INFINITE);
        std::cout << "[PID: " << pid << "] Connection established. Starting download of '" << fileName << "'...\n";
        ReleaseMutex(hMutex);
        BracketResult result = PerformTaskLogic();
        int sleepTime = (rand() % kMaxDownloadDelay + kMinDownloadDelay) * 1000;
        Sleep(sleepTime);
        WaitForSingleObject(hMutex, INFINITE);
        std::cout << "[PID: " << pid << "] File '" << fileName << "' processed successfully.\n";
        std::cout << "       -> Analysis Result: Open Brackets: " << result.openCount
            << ", Close Brackets: " << result.closeCount << "\n";
        ReleaseMutex(hMutex);
        ReleaseSemaphore(hSemaphore, 1, nullptr);
    }
    CloseHandle(hSemaphore);
    CloseHandle(hMutex);
    CloseHandle(hExitEvent);
    return 0;
}