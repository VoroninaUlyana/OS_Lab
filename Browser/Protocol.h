#pragma once
#include <windows.h>
const int MAX_DATA_SIZE = 100;
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