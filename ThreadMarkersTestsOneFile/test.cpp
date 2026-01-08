#include "pch.h"
#include <windows.h>
#include <vector>
#include <iostream>
#include <string>
#include <sstream>
#include <gtest/gtest.h>
using namespace std;

struct ThreadParams
{
    int id;
    int arraySize;
    vector<int>* pArray;
    CRITICAL_SECTION* pCS;
    HANDLE hStartEvent;
    HANDLE hContinueEvent;
    HANDLE hStoppedEvent;
    HANDLE hFinishEvent;
    HANDLE hThreadExitedEvent;
};

class Simulation
{
public:
    Simulation() : hStartEvent(NULL), hContinueEvent(NULL)
    {
        InitializeCriticalSection(&cs);
    }

    ~Simulation()
    {
        cleanup();
        DeleteCriticalSection(&cs);
    }

    bool Start(int arraySize, int numMarkers)
    {
        if (arraySize <= 0 || numMarkers <= 0) return false;
        this->arraySize = arraySize;
        this->numMarkers = numMarkers;
        sharedArray.assign(arraySize, 0);

        hStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        hContinueEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        stoppedEvents.resize(numMarkers);
        finishEvents.resize(numMarkers);
        threadHandles.resize(numMarkers);
        threadIds.resize(numMarkers);
        active.resize(numMarkers, true);

        for (int i = 0; i < numMarkers; ++i)
        {
            stoppedEvents[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
            finishEvents[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
            ThreadParams* tp = new ThreadParams();
            tp->id = i + 1;
            tp->arraySize = arraySize;
            tp->pArray = &sharedArray;
            tp->pCS = &cs;
            tp->hStartEvent = hStartEvent;
            tp->hContinueEvent = hContinueEvent;
            tp->hStoppedEvent = stoppedEvents[i];
            tp->hFinishEvent = finishEvents[i];
            tp->hThreadExitedEvent = NULL;

            DWORD tid;
            HANDLE h = CreateThread(NULL, 0, &Simulation::MarkerThreadProcStatic, tp, 0, &tid);
            if (h == NULL)
            {
                cerr << "CreateThread failed for marker " << (i + 1) << endl;
                delete tp;
                return false;
            }
            threadHandles[i] = h;
            threadIds[i] = tid;
        }

        SetEvent(hStartEvent);
        return true;
    }

    void WaitAllBlocked()
    {
        vector<HANDLE> handles;
        for (int i = 0; i < numMarkers; ++i)
        {
            if (active[i])
                handles.push_back(stoppedEvents[i]);
        }
        if (handles.empty()) return;
        WaitForMultipleObjects((DWORD)handles.size(), handles.data(), TRUE, INFINITE);
    }

    void TerminateMarker(int markerNumber)
    {
        if (markerNumber < 1 || markerNumber > numMarkers) return;
        int idx = markerNumber - 1;
        if (!active[idx]) return;
        SetEvent(finishEvents[idx]);
        if (threadHandles[idx])
        {
            WaitForSingleObject(threadHandles[idx], INFINITE);
            CloseHandle(threadHandles[idx]);
            threadHandles[idx] = NULL;
        }
        active[idx] = false;
        ResetEvent(stoppedEvents[idx]);
        ResetEvent(finishEvents[idx]);
    }

    void ContinueAll()
    {
        for (int i = 0; i < numMarkers; ++i)
        {
            if (active[i])
                ResetEvent(stoppedEvents[i]);
        }
        SetEvent(hContinueEvent);
        Sleep(1);
        ResetEvent(hContinueEvent);
    }

    vector<int> GetArraySnapshot()
    {
        vector<int> copy;
        EnterCriticalSection(&cs);
        copy = sharedArray;
        LeaveCriticalSection(&cs);
        return copy;
    }

    vector<bool> GetActiveMarkers() { return active; }
    int GetNumMarkers() const { return numMarkers; }
    int GetArraySize() const { return arraySize; }

private:
    int arraySize = 0;
    int numMarkers = 0;
    vector<int> sharedArray;
    CRITICAL_SECTION cs;
    HANDLE hStartEvent;
    HANDLE hContinueEvent;
    vector<HANDLE> stoppedEvents;
    vector<HANDLE> finishEvents;
    vector<HANDLE> threadHandles;
    vector<DWORD> threadIds;
    vector<bool> active;

    void cleanup()
    {
        if (hStartEvent) { CloseHandle(hStartEvent); hStartEvent = NULL; }
        if (hContinueEvent) { CloseHandle(hContinueEvent); hContinueEvent = NULL; }
        for (auto& h : stoppedEvents) if (h) { CloseHandle(h); h = NULL; }
        for (auto& h : finishEvents) if (h) { CloseHandle(h); h = NULL; }
    }

    static DWORD WINAPI MarkerThreadProcStatic(LPVOID lpParam)
    {
        ThreadParams* tp = static_cast<ThreadParams*>(lpParam);
        if (!tp) return 0;
        DWORD res = MarkerThreadProc(tp);
        delete tp;
        return res;
    }

    static DWORD MarkerThreadProc(ThreadParams* tp)
    {
        int id = tp->id;
        int n = tp->arraySize;
        vector<int>* pArray = tp->pArray;
        CRITICAL_SECTION* pCS = tp->pCS;
        HANDLE hStartEvent = tp->hStartEvent;
        HANDLE hContinueEvent = tp->hContinueEvent;
        HANDLE hStoppedEvent = tp->hStoppedEvent;
        HANDLE hFinishEvent = tp->hFinishEvent;

        WaitForSingleObject(hStartEvent, INFINITE);
        srand(GetTickCount() + id); // важно!

        int marks = 0;
        int blockedIndex = -1;
        for (;;)
        {
            // Проверка сигнала завершения
            if (WaitForSingleObject(hFinishEvent, 0) == WAIT_OBJECT_0)
            {
                EnterCriticalSection(pCS);
                for (int i = 0; i < n; ++i)
                    if ((*pArray)[i] == id) (*pArray)[i] = 0;
                LeaveCriticalSection(pCS);
                return 0;
            }

            int idx = rand() % n;

            EnterCriticalSection(pCS);
            if ((*pArray)[idx] == 0)
            {
                LeaveCriticalSection(pCS);
                Sleep(5);
                EnterCriticalSection(pCS);
                if ((*pArray)[idx] == 0)
                {
                    (*pArray)[idx] = id;
                    LeaveCriticalSection(pCS);
                    Sleep(5);
                }
                else LeaveCriticalSection(pCS);
            }
            else LeaveCriticalSection(pCS);

            // Если не нашли свободной ячейки — сигнализируем main
            if ((*pArray)[idx] != 0)
                SetEvent(hStoppedEvent);

            // Ждём события Continue или Finish
            HANDLE waitHandles[2] = { hContinueEvent, hFinishEvent };
            DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
            if (waitRes == WAIT_OBJECT_0 + 1) // Finish
            {
                EnterCriticalSection(pCS);
                for (int i = 0; i < n; ++i)
                    if ((*pArray)[i] == id) (*pArray)[i] = 0;
                LeaveCriticalSection(pCS);
                return 0;
            }
        }


        SetEvent(hStoppedEvent);
        HANDLE waitHandles[2] = { hContinueEvent, hFinishEvent };
        DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        WaitForSingleObject(hFinishEvent, INFINITE);

        // Очистка всех меток текущего маркера
        EnterCriticalSection(pCS);
        for (int i = 0; i < n; ++i)
        {
            if ((*pArray)[i] == id)
                (*pArray)[i] = 0;
        }
        LeaveCriticalSection(pCS);

        return 0;

    }
};

// -------------------------
//        ТЕСТЫ
// -------------------------
TEST(MarkerBasic, Test1_1_SingleThreadCorrectMarking)
{
    Simulation sim;
    ASSERT_TRUE(sim.Start(10, 1));
    sim.WaitAllBlocked();
    auto v = sim.GetArraySnapshot();

    int count = 0;
    for (int x : v) if (x == 1) count++;
    EXPECT_GT(count, 0);  // должно быть хотя бы одно помеченное значение
}

TEST(MarkerBasic, Test1_2_TerminateWorks)
{
    Simulation sim;
    ASSERT_TRUE(sim.Start(10, 2));
    sim.WaitAllBlocked();
    sim.TerminateMarker(1);
    auto v = sim.GetArraySnapshot();

    for (int x : v)
        EXPECT_NE(x, 1); // после завершения маркера 1 не должно остаться его следов
}
// -------------------------
//  Тест 2.1: Отсутствие гонки за ресурс
// -------------------------
TEST(MarkerConcurrency, Test2_1_NoRaceCondition)
{
    Simulation sim;
    int arraySize = 20;
    int numMarkers = 10;
    ASSERT_TRUE(sim.Start(arraySize, numMarkers));

    // Ждём блокировки всех потоков
    sim.WaitAllBlocked();

    auto arr = sim.GetArraySnapshot();

    int markedCount = 0;
    for (int val : arr)
    {
        // Элемент должен быть либо 0, либо номером потока (1..10)
        EXPECT_TRUE(val == 0 || (val >= 1 && val <= numMarkers));
        if (val != 0) markedCount++;
    }

    // Проверка: общее количество помеченных ячеек <= размер массива
    EXPECT_LE(markedCount, arraySize);
}

// -------------------------
//  Тест 2.2: Корректное поочередное завершение
// -------------------------
TEST(MarkerConcurrency, Test2_2_TerminateSequentially)
{
    Simulation sim;
    int arraySize = 30;
    int numMarkers = 5;
    ASSERT_TRUE(sim.Start(arraySize, numMarkers));

    sim.WaitAllBlocked();

    for (int i = numMarkers; i >= 1; --i)
    {
        sim.WaitAllBlocked();

        sim.TerminateMarker(i);

        auto arr = sim.GetArraySnapshot();
        for (int val : arr)
            EXPECT_NE(val, i);

        sim.ContinueAll();
    }

    auto finalArr = sim.GetArraySnapshot();
    for (int val : finalArr)
        EXPECT_EQ(val, 0);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
