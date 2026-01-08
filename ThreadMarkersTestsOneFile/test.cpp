#include "pch.h"
#include <windows.h>
#include <vector>
#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>
#include <gtest/gtest.h>
static const int SLEEP_DURATION_MS = 5;
class SimulationException : public std::runtime_error 
{
public:
    explicit SimulationException(const std::string& message) : std::runtime_error(message) {}
};
struct ThreadParams
{
    int id;
    int arraySize;
    std::vector<int>* pArray;
    CRITICAL_SECTION* pCS;
    HANDLE hStartEvent;
    HANDLE hContinueEvent;
    HANDLE hStoppedEvent;
    HANDLE hFinishEvent;
    ThreadParams() : id(0), arraySize(0), pArray(nullptr), pCS(nullptr),
        hStartEvent(nullptr), hContinueEvent(nullptr),
        hStoppedEvent(nullptr), hFinishEvent(nullptr) {}
};

class Simulation
{
public:
    Simulation() : hStartEvent(nullptr), hContinueEvent(nullptr), arraySize(0), numMarkers(0)
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
        hStartEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        hContinueEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (nullptr == hStartEvent || nullptr == hContinueEvent)
        {
            return false;
        }
        stoppedEvents.resize(numMarkers);
        finishEvents.resize(numMarkers);
        threadHandles.resize(numMarkers);
        threadIds.resize(numMarkers);
        active.assign(numMarkers, true);
        for (int i = 0; i < numMarkers; ++i)
        {
            stoppedEvents[i] = CreateEvent(nullptr, TRUE, FALSE, nullptr);
            finishEvents[i] = CreateEvent(nullptr, TRUE, FALSE, nullptr);
            ThreadParams* tp = new ThreadParams();
            tp->id = i + 1;
            tp->arraySize = arraySize;
            tp->pArray = &sharedArray;
            tp->pCS = &cs;
            tp->hStartEvent = hStartEvent;
            tp->hContinueEvent = hContinueEvent;
            tp->hStoppedEvent = stoppedEvents[i];
            tp->hFinishEvent = finishEvents[i];
            DWORD tid;
            HANDLE h = CreateThread(nullptr, 0, &Simulation::MarkerThreadProcStatic, tp, 0, &tid);
            if (nullptr == h)
            {
                std::cerr << "CreateThread failed for marker " << (i + 1) << std::endl;
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
        std::vector<HANDLE> handles;
        for (int i = 0; i < numMarkers; ++i)
        {
            if (active[i])
                handles.push_back(stoppedEvents[i]);
        }
        if (handles.empty()) return;
        WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), TRUE, INFINITE);
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
            threadHandles[idx] = nullptr;
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
    std::vector<int> GetArraySnapshot()
    {
        EnterCriticalSection(&cs);
        std::vector<int> copy = sharedArray;
        LeaveCriticalSection(&cs);
        return copy;
    }
    std::vector<bool> GetActiveMarkers() { return active; }
    int GetNumMarkers() const { return numMarkers; }
    int GetArraySize() const { return arraySize; }
private:
    int arraySize = 0;
    int numMarkers = 0;
    std::vector<int> sharedArray;
    CRITICAL_SECTION cs;
    HANDLE hStartEvent;
    HANDLE hContinueEvent;
    std::vector<HANDLE> stoppedEvents;
    std::vector<HANDLE> finishEvents;
    std::vector<HANDLE> threadHandles;
    std::vector<DWORD> threadIds;
    std::vector<bool> active;
    void cleanup()
    {
        if (hStartEvent) 
        { 
            CloseHandle(hStartEvent); 
            hStartEvent = nullptr; 
        }
        if (hContinueEvent) 
        { 
            CloseHandle(hContinueEvent); 
            hContinueEvent = nullptr; 
        }
        for (auto& h : stoppedEvents) if (h) { CloseHandle(h); h = nullptr; }
        for (auto& h : finishEvents) if (h) { CloseHandle(h); h = nullptr; }
    }

    static DWORD WINAPI MarkerThreadProcStatic(LPVOID lpParam)
    {
        ThreadParams* tp = static_cast<ThreadParams*>(lpParam);
        if (nullptr == tp)
        {
            return 1;
        }
        DWORD res = MarkerThreadProc(tp);
        delete tp;
        return res;
    }

    static DWORD MarkerThreadProc(ThreadParams* tp)
    {
        const int id = tp->id;
        const int n = tp->arraySize;
        std::vector<int>* pArray = tp->pArray;
        CRITICAL_SECTION* pCS = tp->pCS;
        HANDLE hStartEvent = tp->hStartEvent;
        HANDLE hContinueEvent = tp->hContinueEvent;
        HANDLE hStoppedEvent = tp->hStoppedEvent;
        HANDLE hFinishEvent = tp->hFinishEvent;
        WaitForSingleObject(hStartEvent, INFINITE);
        srand(static_cast<unsigned int>(GetTickCount() + id));
        int marks = 0;
        int blockedIndex = -1;
        int idx = 0;
        while (true)
        {
            idx = rand() % n;
            EnterCriticalSection(pCS);
            if ((*pArray)[idx] == 0)
            {
                LeaveCriticalSection(pCS);
                Sleep(SLEEP_DURATION_MS);
                EnterCriticalSection(pCS);
                if (0 == (*pArray)[idx])
                {
                    (*pArray)[idx] = id;
                    marks++;
                    LeaveCriticalSection(pCS);
                    Sleep(SLEEP_DURATION_MS);
                    continue;
                }
            }
            std::cout << "Marker " << id << " blocked at index " << idx << ". Total marks: " << marks << "\n";
            LeaveCriticalSection(pCS);
            SetEvent(hStoppedEvent);
            HANDLE waitHandles[2] = { hContinueEvent, hFinishEvent };
            DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
            if (waitRes == WAIT_OBJECT_0 + 1)
            {
                EnterCriticalSection(pCS);
                for (int i = 0; i < n; ++i)
                {
                    if ((*pArray)[i] == id) (*pArray)[i] = 0;
                }
                LeaveCriticalSection(pCS);
                return 0;
            }
            marks = 0;
        }
    }
};
TEST(MarkerBasic, Test1_1_SingleThreadCorrectMarking)
{
    Simulation sim;
    ASSERT_TRUE(sim.Start(10, 1));
    sim.WaitAllBlocked();
    auto v = sim.GetArraySnapshot();
    int count = 0;
    for (int x : v) if (x == 1) count++;
    EXPECT_GT(count, 0);
}

TEST(MarkerBasic, Test1_2_TerminateWorks)
{
    Simulation sim;
    ASSERT_TRUE(sim.Start(10, 2));
    sim.WaitAllBlocked();
    sim.TerminateMarker(1);
    auto v = sim.GetArraySnapshot();
    for (int x : v) EXPECT_NE(x, 1);
}

TEST(MarkerConcurrency, Test2_1_NoRaceCondition)
{
    Simulation sim;
    int numMarkers = 10;
    ASSERT_TRUE(sim.Start(20, numMarkers));
    sim.WaitAllBlocked();
    auto arr = sim.GetArraySnapshot();
    int markedCount = 0;
    for (int val : arr)
    {
        EXPECT_TRUE(val == 0 || (val >= 1 && val <= numMarkers));
        if (val != 0) markedCount++;
    }
    EXPECT_LE(markedCount, 20);
}

TEST(MarkerConcurrency, Test2_2_TerminateSequentially)
{
    Simulation sim;
    int numMarkers = 5;
    ASSERT_TRUE(sim.Start(30, numMarkers));
    sim.WaitAllBlocked();
    for (int i = numMarkers; i >= 1; --i)
    {
        sim.WaitAllBlocked();
        sim.TerminateMarker(i);
        auto arr = sim.GetArraySnapshot();
        for (int val : arr) EXPECT_NE(val, i);
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
