#include <iostream>
#include <windows.h>
#include <vector>
#include <string>
#include "../Browser/Protocol.h" // Путь к хедеру

using namespace std;

// Структура для хранения дескрипторов одного воркера
struct WorkerInfo {
    HANDLE hPipeIn;      // Browser пишет сюда (задачи)
    HANDLE hPipeOut;     // Browser читает отсюда (результаты)
    PROCESS_INFORMATION pi; // Информация о процессе (чтобы ждать завершения)
};

int main() {
    int N, M;
    cout << "Enter number of workers (N): ";
    cin >> N;
    cout << "Enter number of tasks (M): ";
    cin >> M;

    vector<WorkerInfo> workers(N);

    // --- Этап запуска воркеров ---
    for (int i = 0; i < N; ++i) {
        // 1. Формируем имена каналов
        wstring pipeNameIn = L"\\\\.\\pipe\\worker_in_" + to_wstring(i);
        wstring pipeNameOut = L"\\\\.\\pipe\\worker_out_" + to_wstring(i);

        // 2. Создаем каналы
        // Канал IN: Browser пишет (OUTBOUND), Worker читает
        workers[i].hPipeIn = CreateNamedPipe(
            pipeNameIn.c_str(),
            PIPE_ACCESS_OUTBOUND, // Направление данных: от нас к ним
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1, // Макс. 1 экземпляр
            sizeof(Task), // Размер буфера вывода
            sizeof(Task), // Размер буфера ввода
            0, NULL);

        // Канал OUT: Browser читает (INBOUND), Worker пишет
        workers[i].hPipeOut = CreateNamedPipe(
            pipeNameOut.c_str(),
            PIPE_ACCESS_INBOUND, // Направление данных: от них к нам
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,
            sizeof(Result),
            sizeof(Result),
            0, NULL);

        if (workers[i].hPipeIn == INVALID_HANDLE_VALUE || workers[i].hPipeOut == INVALID_HANDLE_VALUE) {
            cerr << "Error creating pipes for worker " << i << endl;
            return 1;
        }

        // 3. Запускаем Worker.exe
        STARTUPINFO si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&workers[i].pi, sizeof(workers[i].pi));

        // Командная строка: "Worker.exe {ID}"
        // Важно: путь должен быть корректным. Если запускаешь из VS, Worker.exe должен быть найден.
        // Обычно VS кладет их в Debug/Release папку. Для простоты будем считать, что они рядом.
        wstring cmdLine = L"Worker.exe " + to_wstring(i);

        // CreateProcess требует изменяемую строку (LPWSTR), поэтому копируем в буфер
        vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
        cmdBuffer.push_back(0);

        if (!CreateProcess(
            NULL,               // Имя модуля (ищем по cmdLine)
            cmdBuffer.data(),   // Командная строка
            NULL, NULL,         // Атрибуты защиты
            FALSE,              // Наследовать дескрипторы
            CREATE_NEW_CONSOLE, // Создать новое окно для воркера (чтобы видеть логи)
            NULL, NULL,         // Окружение и текущая папка
            &si,                // Startup info
            &workers[i].pi      // Process info (сюда вернется handle процесса)
        )) {
            cerr << "Failed to launch Worker " << i << ". Error: " << GetLastError() << endl;
            return 1;
        }

        // Ждем, пока Worker подключится к каналам
        // (Worker вызывает CreateFile, а мы здесь ждем этого события)
        ConnectNamedPipe(workers[i].hPipeIn, NULL);
        ConnectNamedPipe(workers[i].hPipeOut, NULL);
    }

    cout << "All workers started and connected." << endl;

    // ... Продолжение следует ...

    // (Временная заглушка для компиляции: закрытие хендлов)
    for (int i = 0; i < N; ++i) {
        CloseHandle(workers[i].hPipeIn);
        CloseHandle(workers[i].hPipeOut);
        CloseHandle(workers[i].pi.hProcess);
        CloseHandle(workers[i].pi.hThread);
    }

    return 0;
}