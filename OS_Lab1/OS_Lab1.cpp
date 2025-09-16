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
    cout << "Worker thread started. Implementation pending..." << endl;
    ThreadData* data = (ThreadData*)param;
    delete[] data->array;
    delete data;
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
    ThreadData* data = new ThreadData;
    data->array = arr;
    data->size = size;
    HANDLE hThread = (HANDLE)_beginthreadex(
        nullptr,    
        0,         
        &workerThread, 
        (void*)data,   
        CREATE_SUSPENDED, 
        nullptr     
    );
    if (hThread == NULL) 
    {
        cerr << "Failed to create thread!" << endl;
        delete[] arr;
        delete data;
        return 1;
    }
    cout << "Thread created in suspended state. Suspending for " << suspendTime << " ms..." << endl;
    Sleep(suspendTime);
    ResumeThread(hThread);
    cout << "Thread resumed!" << endl;
    cout << "Main thread waiting for worker to finish..." << endl;
    WaitForSingleObject(hThread, INFINITE);
    cout << "Closing thread handle..." << endl;
    CloseHandle(hThread);
    cout << "Main thread exiting." << endl;

    return 0;
}