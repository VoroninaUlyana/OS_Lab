#include <windows.h>
#include <process.h>
#include <iostream>
#include <cstdlib>
#include <ctime>
using namespace std;

struct ThreadData 
{
    int* array;
    int size;
};

unsigned __stdcall workerThread(void* param) 
{
    ThreadData* data = (ThreadData*)param;
    int oddCount = 0;
    cout << "\nWorker Thread: Processing array..." << endl;
    for (int i = 0; i < data->size; ++i) 
    {
        if (data->array[i] % 2 != 0) 
        { 
            oddCount++;
        }
        Sleep(100);
    }
    cout << "Worker Thread: Number of odd elements is " << oddCount << endl;
    delete[] data->array;
    delete data;

    return 0;
}

DWORD WINAPI workerThreadWin(LPVOID param) 
{
    workerThread(param);
    return 0;
}

int main() 
{
    int size;
    DWORD suspendTime;
    int choice;
    cout << "Enter array size: ";
    cin >> size;
    if (size <= 0) 
    {
        cerr << "Invalid array size!" << endl;
        return 1;
    }
    int* arr = new int[size];
    cout << "Generate array randomly? (1 - Yes, 0 - No): ";
    cin >> choice;
    if (choice == 1) 
    {
        srand(time(nullptr));
        cout << "Generated array: ";
        for (int i = 0; i < size; ++i) 
        {
            arr[i] = rand() % 100 - 50;
            cout << arr[i] << " ";
        }
        cout << endl;
    }
    else 
    {
        cout << "Enter " << size << " elements:" << endl;
        for (int i = 0; i < size; ++i) 
        {
            cin >> arr[i];
        }
    }
    cout << "Enter suspend time for worker thread (ms): ";
    cin >> suspendTime;
    int method;
    cout << "Choose creation method (1 - _beginthreadex, 2 - CreateThread): ";
    cin >> method;
    ThreadData* data = new ThreadData;
    data->array = arr;
    data->size = size;
    HANDLE hThread = NULL;
    unsigned initFlags = CREATE_SUSPENDED;
    if (method == 1) 
    {
        hThread = (HANDLE)_beginthreadex(nullptr, 0, workerThread, data, initFlags, nullptr);
    }
    else 
    {
        hThread = CreateThread(nullptr, 0, workerThreadWin, data, initFlags, nullptr);
    }
    cout << "Suspending for " << suspendTime << " ms..." << endl;
    Sleep(suspendTime);
    ResumeThread(hThread);
    cout << "Thread resumed with ResumeThread!" << endl;
    cout << "Main thread waiting for worker to finish..." << endl;
    WaitForSingleObject(hThread, INFINITE);
    cout << "Closing thread handle..." << endl;
    CloseHandle(hThread);
    cout << "Main thread exiting." << endl;

    return 0;
}