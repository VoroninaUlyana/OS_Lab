#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
using namespace std;

void parent_mode(const char* exePath);
void child_mode();

int main(int argc, char* argv[]) 
{
    setlocale(LC_ALL, "Rus");
    if (argc > 1 && string(argv[1]) == "child") 
    {
        cout << "[Child] Запущен дочерний процесс.\n";
        child_mode();
    }
    else 
    {
        cout << "[Parent] Запущен родительский процесс.\n";
        parent_mode(argv[0]);
    }
    return 0;
}

void parent_mode(const char* exePath) 
{
    HANDLE hPipeParentToChildRead = NULL, hPipeParentToChildWrite = NULL;
    HANDLE hPipeChildToParentRead = NULL, hPipeChildToParentWrite = NULL;
    SECURITY_ATTRIBUTES saAttr{};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
    if (!CreatePipe(&hPipeParentToChildRead, &hPipeParentToChildWrite, &saAttr, 0)) 
    {
        cerr << "[Parent] Ошибка CreatePipe (1).\n";
        return;
    }
    if (!CreatePipe(&hPipeChildToParentRead, &hPipeChildToParentWrite, &saAttr, 0)) 
    {
        cerr << "[Parent] Ошибка CreatePipe (2).\n";
        return;
    }
    SetHandleInformation(hPipeParentToChildWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hPipeChildToParentRead, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdInput = hPipeParentToChildRead;
    si.hStdOutput = hPipeChildToParentWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;
    ostringstream oss;
    oss << "\"" << exePath << "\" child";
    string command = oss.str();
    cout << "[Parent] Создаю дочерний процесс...\n";
    BOOL success = CreateProcessA(
        NULL, &command[0],
        NULL, NULL, TRUE,
        0, NULL, NULL,
        &si, &pi
    );
    if (!success)
    {
        cerr << "[Parent] Ошибка CreateProcess.\n";
        return;
    }
    cout << "[Parent] Дочерний процесс создан успешно.\n";
    int n;
    cout << "[Parent] Введите размер массива: ";
    cin >> n;
    vector<int> arr(n);
    cout << "[Parent] Введите элементы массива:\n";
    for (int i = 0; i < n; ++i)
    {
        cin >> arr[i];
    }
    DWORD written;
    WriteFile(hPipeParentToChildWrite, &n, sizeof(int), &written, NULL);
    WriteFile(hPipeParentToChildWrite, arr.data(), n * sizeof(int), &written, NULL);
    cout << "[Parent] Данные отправлены дочернему процессу.\n";
    CloseHandle(hPipeParentToChildWrite);
    int evenCount = 0;
    DWORD readBytes;
    if (ReadFile(hPipeChildToParentRead, &evenCount, sizeof(int), &readBytes, NULL))
    {
        cout << "[Parent] Получен результат от дочернего процесса.\n";
        cout << "Количество чётных элементов: " << evenCount << "\n";
    }
    else 
    {
        cerr << "[Parent] Ошибка при чтении из канала.\n";
    }
    CloseHandle(hPipeChildToParentRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    cout << "[Parent] Работа завершена.\n";
}

void child_mode() 
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    int n;
    DWORD bytesRead;
    if (!ReadFile(hStdin, &n, sizeof(int), &bytesRead, NULL) || bytesRead == 0) 
    {
        cerr << "[Child] Ошибка чтения размера массива.\n";
        return;
    }
    vector<int> arr(n);
    if (!ReadFile(hStdin, arr.data(), n * sizeof(int), &bytesRead, NULL)) 
    {
        cerr << "[Child] Ошибка чтения данных массива.\n";
        return;
    }
    cout << "[Child] Получен массив. Обработка данных...\n";
    int evenCount = 0;
    for (int x : arr)
    {
        if (x % 2 == 0)
        {
            evenCount++;
        }
    }
    cout << "[Child] Результат вычислен. Отправка родителю...\n";
    DWORD bytesWritten;
    WriteFile(hStdout, &evenCount, sizeof(int), &bytesWritten, NULL);
    cout << "[Child] Завершение дочернего процесса.\n";
}