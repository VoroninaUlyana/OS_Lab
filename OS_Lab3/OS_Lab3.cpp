#include <windows.h>
#include <vector>
#include <iostream>
#include <string>
#include <sstream>
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
    Simulation() :
        hStartEvent(NULL), hContinueEvent(NULL)
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
        for (int i = 0; i < numMarkers; ++i) {
            stoppedEvents[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
            finishEvents[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
            threadHandles[i] = NULL;
            threadIds[i] = 0;
            active.push_back(true);
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
            tp->hThreadExitedEvent = NULL;
            DWORD tid;
            HANDLE h = CreateThread(NULL, 0, &Simulation::MarkerThreadProcStatic, tp, 0, &tid);
            if (h == NULL)
            {
                cerr << "CreateThread failed for marker " << (i + 1) << " code=" << GetLastError() << endl;
                delete tp;
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
        vector<HANDLE> handles;
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
            {
                ResetEvent(stoppedEvents[i]);
            }
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
    void WaitAllExited()
    {
        for (int i = 0; i < numMarkers; ++i)
        {
            if (threadHandles[i])
            {
                WaitForSingleObject(threadHandles[i], INFINITE);
                CloseHandle(threadHandles[i]);
                threadHandles[i] = NULL;
            }
        }
    }
    void cleanup()
    {
        if (hStartEvent) { CloseHandle(hStartEvent); hStartEvent = NULL; }
        if (hContinueEvent) { CloseHandle(hContinueEvent); hContinueEvent = NULL; }
        for (auto& h : stoppedEvents) if (h) { CloseHandle(h); h = NULL; }
        for (auto& h : finishEvents) if (h) { CloseHandle(h); h = NULL; }
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
        srand(id);
        int marks = 0;
        while (true)
        {
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
                    marks++;
                    LeaveCriticalSection(pCS);
                    Sleep(5);
                    continue; 
                }
            }
            cout << "Marker " << id << " blocked at index " << idx << ". Total marks: " << marks << "\n";
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
        throw runtime_error("Failed to start simulation");
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

void PrintArray(const vector<int>& arr)
{
    cout << "Array: [";
    for (size_t i = 0; i < arr.size(); ++i)
    {
        if (i) cout << ", ";
        cout << arr[i];
    }
    cout << "]\n";
}
int main()
{
    cout << "Simulation (markers, critical section & events)\n";
    int arraySize = 0;
    cout << "Enter array size: ";
    cin >> arraySize;
    int N = 0;
    cout << "Enter number of markers: ";
    cin >> N;
    Simulation sim;
    if (!sim.Start(arraySize, N))
    {
        cerr << "Failed to start simulation\n";
        return 1;
    }
    while (true)
    {
        auto act = sim.GetActiveMarkers();
        bool anyActive = false;
        for (bool b : act) if (b) { anyActive = true; break; }
        if (!anyActive) break;
        cout << "Waiting for all active markers to block...\n";
        sim.WaitAllBlocked();
        auto snapshot = sim.GetArraySnapshot();
        PrintArray(snapshot);
        cout << "Enter marker number to terminate (1.." << sim.GetNumMarkers() << "): ";
        int toTerminate = 0;
        cin >> toTerminate;
        sim.TerminateMarker(toTerminate);
        auto snap2 = sim.GetArraySnapshot();
        cout << "After termination and cleanup:\n";
        PrintArray(snap2);
        sim.ContinueAll();
    }
    cout << "All markers finished. Final array:\n";
    PrintArray(sim.GetArraySnapshot());
    sim.WaitAllExited();
    return 0;
}