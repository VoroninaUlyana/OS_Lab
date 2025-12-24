#include <iostream>
#include <windows.h>
#include <vector>
#include <string>
#include "C:/OS/Lab5/Browser/Protocol.h"
using namespace std;
struct WorkerInfo 
{
    HANDLE hPipeIn;      
    HANDLE hPipeOut;     
    PROCESS_INFORMATION pi; 
};
void SafePrint(HANDLE hMutex, const string& msg) 
{
    WaitForSingleObject(hMutex, INFINITE);
    cout << msg;
    ReleaseMutex(hMutex);
}
int main() 
{
    HANDLE hConsoleMutex = CreateMutex(NULL, FALSE, CONSOLE_MUTEX_NAME.c_str());
    int N, M;
    cout << "Enter number of workers (N): ";
    cin >> N;
    cout << "Enter number of tasks (M): ";
    cin >> M;
    vector<WorkerInfo> workers(N);
    for (int i = 0; i < N; ++i) 
    {
        wstring pipeNameIn = L"\\\\.\\pipe\\worker_in_" + to_wstring(i);
        wstring pipeNameOut = L"\\\\.\\pipe\\worker_out_" + to_wstring(i);
        workers[i].hPipeIn = CreateNamedPipe(
            pipeNameIn.c_str(),
            PIPE_ACCESS_OUTBOUND,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,
            sizeof(Task),
            sizeof(Task), 
            0, NULL);
        workers[i].hPipeOut = CreateNamedPipe(
            pipeNameOut.c_str(),
            PIPE_ACCESS_INBOUND, 
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,
            sizeof(Result),
            sizeof(Result),
            0, NULL);
        if (workers[i].hPipeIn == INVALID_HANDLE_VALUE || workers[i].hPipeOut == INVALID_HANDLE_VALUE) 
        {
            cerr << "Error creating pipes for worker " << i << endl;
            return 1;
        }
        STARTUPINFO si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&workers[i].pi, sizeof(workers[i].pi));
        wstring cmdLine = L"Worker.exe " + to_wstring(i);
        vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
        cmdBuffer.push_back(0);
        if (!CreateProcess(
            NULL,               
            cmdBuffer.data(),   
            NULL, NULL,        
            FALSE,              
            0, 
            NULL, NULL,        
            &si,                
            &workers[i].pi     
        )) {
            cerr << "Failed to launch Worker " << i << ". Error: " << GetLastError() << endl;
            return 1;
        }
        ConnectNamedPipe(workers[i].hPipeIn, NULL);
        ConnectNamedPipe(workers[i].hPipeOut, NULL);
    }
    WaitForSingleObject(hConsoleMutex, INFINITE);
    cout << "All workers started and connected." << endl;
    cout << "Starting task distribution..." << endl;
    ReleaseMutex(hConsoleMutex);
    for (int i = 0; i < M; ++i) 
    {
        int workerIndex = i % N; 
        Task task;
        task.type = TASK_CALCULATE;
        task.size = 5 + (rand() % 10); 
        WaitForSingleObject(hConsoleMutex, INFINITE);
        cout << "\n[Task " << i << "] Sending to Worker " << workerIndex
            << " (Array size: " << task.size << ")...";
        cout << "   Data: [ "; 
        for (int k = 0; k < task.size; ++k)
        {
            task.data[k] = (double)(rand() % 100);
            cout << task.data[k];
            if (k < task.size - 1) 
            {
                cout << ", ";
            }
        }
        cout << " ]" << endl; 
        ReleaseMutex(hConsoleMutex);
        DWORD bytesWritten;
        if (!WriteFile(workers[workerIndex].hPipeIn, &task, sizeof(Task), &bytesWritten, NULL)) 
        {
            cerr << "Failed to send task to worker " << workerIndex << endl;
            break;
        }
        Result res;
        DWORD bytesRead;
        if (ReadFile(workers[workerIndex].hPipeOut, &res, sizeof(Result), &bytesRead, NULL)) 
        {
            WaitForSingleObject(hConsoleMutex, INFINITE);
            cout << " Done." << endl;
            cout << "   -> Median: " << res.median << endl;
            cout << "   -> StdDev: " << res.stdDev << endl;
            ReleaseMutex(hConsoleMutex);
        }
        else 
        {
            cerr << "Failed to read result from worker " << workerIndex << endl;
        }
    }
    cout << "\nAll tasks completed. Shutting down workers..." << endl;
    for (int i = 0; i < N; ++i) 
    {
        Task stopTask;
        stopTask.type = TASK_STOP;
        stopTask.size = 0;
        DWORD written;
        WriteFile(workers[i].hPipeIn, &stopTask, sizeof(Task), &written, NULL);
        WaitForSingleObject(workers[i].pi.hProcess, INFINITE);
        CloseHandle(workers[i].hPipeIn);
        CloseHandle(workers[i].hPipeOut);
        CloseHandle(workers[i].pi.hProcess);
        CloseHandle(workers[i].pi.hThread);
    }
    cout << "Browser finished." << endl;
    CloseHandle(hConsoleMutex);
    system("pause"); 
    return 0;
}