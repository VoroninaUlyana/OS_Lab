#include <windows.h>
#include <process.h>
#include <iostream>
#include <cstdlib>
#include <ctime>

struct ThreadData 
{
    int* array = nullptr;
    int size = 0;
};

void printArray(const int* arr, int size)
{
    std::cout << "Generated array: ";
    for (int i = 0; i < size; ++i)
    {
        std::cout << arr[i] << " ";
    }
    std::cout << std::endl;
}

unsigned __stdcall workerThread(void* param) 
{
    ThreadData* data = static_cast<ThreadData*>(param);
    int oddCount = 0;
    std::cout << "\nWorker Thread: Processing array..." << std::endl;
    for (int i = 0; i < data->size; ++i) 
    {
        if (data->array[i] % 2 != 0) 
        { 
            oddCount++;
        }
        Sleep(100);
    }
    std::cout << "Worker Thread: Number of odd elements is " << oddCount << std::endl;
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
    std::cout << "Enter array size: ";
    std::cin >> size;
    if (0 >= size) 
    {
        std::cerr << "Invalid array size!" << std::endl;
        return 1;
    }
    int* arr = new int[size];
    std::cout << "Generate array randomly? (1 - Yes, 2 - No): ";
    std::cin >> choice;
    if (1 == choice) 
    {
        srand(static_cast<unsigned int>(time(nullptr)));
        std::cout << "Generated array: ";
        for (int i = 0; i < size; ++i) 
        {
            arr[i] = rand() % 100 - 50;
        }
        printArray(arr, size);
    }
    else 
    {
        std::cout << "Enter " << size << " elements:" << std::endl;
        for (int i = 0; i < size; ++i) 
        {
            std::cin >> arr[i];
        }
    }
    std::cout << "Enter suspend time for worker thread (ms): ";
    std::cin >> suspendTime;
    int method;
    std::cout << "Choose creation method (1 - _beginthreadex, 2 - CreateThread): ";
    std::cin >> method;
    ThreadData* data = new ThreadData;
    data->array = arr;
    data->size = size;
    HANDLE hThread = nullptr;
    unsigned initFlags = CREATE_SUSPENDED;
    if (1 == method) 
    {
        hThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, workerThread, data, initFlags, nullptr));
    }
    else 
    {
        hThread = CreateThread(nullptr, 0, workerThreadWin, data, initFlags, nullptr);
    }
    std::cout << "Suspending for " << suspendTime << " ms..." << std::endl;
    Sleep(suspendTime);
    ResumeThread(hThread);
    std::cout << "Thread resumed with ResumeThread!" << std::endl;
    std::cout << "Main thread waiting for worker to finish..." << std::endl;
    WaitForSingleObject(hThread, INFINITE);
    std::cout << "Closing thread handle..." << std::endl;
    CloseHandle(hThread);
    std::cout << "Main thread exiting." << std::endl;

    return 0;
}