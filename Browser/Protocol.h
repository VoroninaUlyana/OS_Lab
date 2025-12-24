#pragma once
#include <windows.h>
#include <string>
const int MAX_DATA_SIZE = 100;
const std::wstring CONSOLE_MUTEX_NAME = L"Global\\Lab5ConsoleMutex";
enum TaskType 
{
    TASK_CALCULATE = 1, 
    TASK_STOP = 2     
};
struct Task 
{
    TaskType type;              
    int size;                   
    double data[MAX_DATA_SIZE]; 
};
struct Result 
{
    double median; 
    double stdDev;
};