#include <windows.h>
#include <vector>
#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>
#include <algorithm>
static const int SLEEP_DURATION_MS = 5;
static const int THREAD_WAIT_TIMEOUT_MS = 1000;
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
    Simulation() :
        hStartEvent(nullptr), hContinueEvent(nullptr), arraySize(0), numMarkers(0)
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
        if (nullptr == hStartEvent)
        {
            throw SimulationException("Failed to create start event");
        }
        hContinueEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (nullptr == hContinueEvent)
        {
            throw SimulationException("Failed to create continue event");
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
            threadHandles[i] = nullptr;
            threadIds[i] = 0;
        }
        for (int i = 0; i < numMarkers; ++i)
        {
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
                DWORD err = GetLastError();
                delete tp;
                throw SimulationException("Failed to create thread " + std::to_string(i + 1) + ". Error: " + std::to_string(err));
            }
            else
            {
                threadHandles[i] = h;
                threadIds[i] = tid;
            }
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
            {
                handles.push_back(stoppedEvents[i]);
            }
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
            {
                ResetEvent(stoppedEvents[i]);
            }
        }
        SetEvent(hContinueEvent);
        Sleep(1);
        ResetEvent(hContinueEvent);
    }
    std::vector<int> GetArraySnapshot()
    {
        std::vector<int> copy;
        EnterCriticalSection(&cs);
        copy = sharedArray;
        LeaveCriticalSection(&cs);
        return copy;
    }
    void WaitAllExited()
    {
        for (int i = 0; i < numMarkers; ++i)
        {
            if (threadHandles[i])
            {
                WaitForSingleObject(threadHandles[i], INFINITE);
                CloseHandle(threadHandles[i]);
                threadHandles[i] = nullptr;
            }
        }
    }
    void cleanup()
    {
        if (hStartEvent) { CloseHandle(hStartEvent); hStartEvent = nullptr; }
        if (hContinueEvent) { CloseHandle(hContinueEvent); hContinueEvent = nullptr; }
        for (auto& h : stoppedEvents) if (h) { CloseHandle(h); h = nullptr; }
        for (auto& h : finishEvents) if (h) { CloseHandle(h); h = nullptr; }
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
        srand(static_cast<unsigned int>(id));
        int marks = 0;
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
Simulation RunSimulationForTests(int arraySize, int numMarkers, bool autoTerminate = true)
{
    Simulation sim;
    if (!sim.Start(arraySize, numMarkers))
    {
        throw std::runtime_error("Failed to start simulation");
    }
    sim.WaitAllBlocked();
    if (autoTerminate)
    {
        for (int i = 1; i <= sim.GetNumMarkers(); ++i)
            sim.TerminateMarker(i);
    }
    sim.WaitAllExited();
    return sim;
}

void PrintArray(const std::vector<int>& arr)
{
    std::cout << "Array: [";
    for (size_t i = 0; i < arr.size(); ++i)
    {
        if (i) std::cout << ", ";
        std::cout << arr[i];
    }
    std::cout << "]\n";
}
int main()
{
    std::cout << "Simulation (markers, critical section & events)\n";
    int arraySize = 0;
    std::cout << "Enter array size: ";
    std::cin >> arraySize;
    int N = 0;
    std::cout << "Enter number of markers: ";
    std::cin >> N;
    Simulation sim;
    if (!sim.Start(arraySize, N))
    {
        std::cerr << "Failed to start simulation\n";
        return 1;
    }
    while (true)
    {
        auto act = sim.GetActiveMarkers();
        bool anyActive = false;
        for (bool b : act) if (b) { anyActive = true; break; }
        if (!anyActive) break;
        std::cout << "Waiting for all active markers to block...\n";
        sim.WaitAllBlocked();
        auto snapshot = sim.GetArraySnapshot();
        PrintArray(snapshot);
        std::cout << "Enter marker number to terminate (1.." << sim.GetNumMarkers() << "): ";
        int toTerminate = 0;
        std::cin >> toTerminate;
        sim.TerminateMarker(toTerminate);
        auto snap2 = sim.GetArraySnapshot();
        std::cout << "After termination and cleanup:\n";
        PrintArray(snap2);
        sim.ContinueAll();
    }
    std::cout << "All markers finished. Final array:\n";
    PrintArray(sim.GetArraySnapshot());
    sim.WaitAllExited();
    return 0;
}