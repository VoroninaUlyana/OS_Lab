#include <iostream>
#include <windows.h>
#include <string>
#include <vector>
#include <algorithm> 
#include <numeric>   
#include <cmath>     
#include "C:/OS/Lab5/Browser/Protocol.h" 
using namespace std;
Result ProcessData(const Task& task) 
{
    Result res = { 0.0, 0.0 };
    if (task.size <= 0) 
    {
        return res; 
    }
    vector<double> v(task.data, task.data + task.size);
    sort(v.begin(), v.end());
    if (task.size % 2 == 0) 
    {
        res.median = (v[task.size / 2 - 1] + v[task.size / 2]) / 2.0;
    }
    else 
    {
        res.median = v[task.size / 2];
    }
    double sum = accumulate(v.begin(), v.end(), 0.0);
    double mean = sum / task.size;
    double sq_sum = 0.0;
    for (double x : v) 
    {
        sq_sum += pow(x - mean, 2);
    }
    res.stdDev = sqrt(sq_sum / task.size);
    return res;
}
int main(int argc, char* argv[]) 
{
    if (argc < 2) 
    {
        cerr << "Worker: No ID provided!" << endl;
        return 1;
    }
    int id = atoi(argv[1]); 
    wstring pipeNameIn = L"\\\\.\\pipe\\worker_in_" + to_wstring(id);
    wstring pipeNameOut = L"\\\\.\\pipe\\worker_out_" + to_wstring(id);
    HANDLE hPipeIn = CreateFile(pipeNameIn.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE hPipeOut = CreateFile(pipeNameOut.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hPipeIn == INVALID_HANDLE_VALUE || hPipeOut == INVALID_HANDLE_VALUE) 
    {
        cerr << "Worker " << id << ": Failed to connect to pipes." << endl;
        return 1;
    }
    wcout << L"Worker " << id << L" started." << endl;
    Task task;
    DWORD bytesRead, bytesWritten;
    bool running = true;
    while (running) 
    {
        if (!ReadFile(hPipeIn, &task, sizeof(Task), &bytesRead, NULL)) 
        {
            break; 
        }
        if (task.type == TASK_STOP) 
        {
            wcout << L"Worker " << id << L": Received STOP signal." << endl;
            running = false;
        }
        else if (task.type == TASK_CALCULATE) 
        {
            Result res = ProcessData(task);
            WriteFile(hPipeOut, &res, sizeof(Result), &bytesWritten, NULL);
        }
    }
    CloseHandle(hPipeIn);
    CloseHandle(hPipeOut);
    return 0;
}