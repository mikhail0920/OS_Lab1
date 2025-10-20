#define _CRT_SECURE_NO_WARNINGS
#include <windows.h> //WinAPI
#include <stdlib.h> //для функции рандома
#include <stdio.h> //для snprintf

#define BUF 1024

static int read_line(HANDLE hStdin, char *out, int maxlen) {
    DWORD n; char c; int pos = 0;
    while (1) {
        if (!ReadFile(hStdin, &c, 1, &n, NULL) || n == 0) return -1;
        if (c == '\r') continue;
        if (c == '\n') break;
        if (pos < maxlen-1) out[pos++] = c;
    }
    out[pos] = 0;
    return pos;
}

int main(void) {
    //дескриптор стандартного консольного ввода
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE) return 1;

    char file1[BUF], file2[BUF], line[BUF];

    const char *p1 = "File for child1:\r\n";
    const char *p2 = "File for child2:\r\n";
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), p1, (DWORD)strlen(p1), NULL, NULL);
    if (read_line(hStdin, file1, BUF) < 0) return 1;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), p2, (DWORD)strlen(p2), NULL, NULL);
    if (read_line(hStdin, file2, BUF) < 0) return 1;

    // создаём два pipe: parent -> child1, parent -> child2
    //pipe - однонаправленный канал между двумя процессами. Имеет конец для чтения и конец для записи
    //SECURITY_ATTRIBUTES структура для параметров безопасности
    //nLength размер; lpSecurityDescriptor указатель на дескриптор безопасности; bInheritHandle = TRUE указывает что дескрипторы канала должны быть унаследованы дочерними процессами. Нужно чтобы родитель мог передать процессу конец чтения канала в качестве его stdin
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE p1_read, p1_write, p2_read, p2_write;
    //&p1_read дескриптор конца чтения; &p1_write дескриптор конца записи; &sa структура безопасности указатель; 0 размер буфера канала по умолчанию
    if (!CreatePipe(&p1_read, &p1_write, &sa, 0)) return 1;
    if (!CreatePipe(&p2_read, &p2_write, &sa, 0)) { CloseHandle(p1_read); CloseHandle(p1_write); return 1; }

    // Создаём дочерние процессы: child.exe <filename>
    // STARTUPINFOA структура с параметрами для создания процесса
    STARTUPINFOA si; 
    // Инфа о процессе, hProcess - дескриптор процесса; hThread - дескриптор основного потока нового процесса; dwProcessId - ID процесса; dwThreadId - ID потока
    PROCESS_INFORMATION pi1, pi2;
    // Обнуляем всю структуру, т.к. некоторые поля должны быть ноль если не юзаются
    ZeroMemory(&si, sizeof(si)); 
    // WinAPI использует это для проверки версии структуры
    si.cb = sizeof(si);
    // dwFlags сообщает ф-и CreateProcessA что она должна использовать значения из полей hStdInput, hStdOutput, hStdError для настройки потоков ввода/вывода дочернего процесса.
    si.dwFlags = STARTF_USESTDHANDLES;
    // child1: stdin <- p1_read
    si.hStdInput = p1_read;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE); 
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    char cmd1[BUF]; snprintf(cmd1, BUF, "child.exe \"%s\"", file1);
    /*
    CreateProcessA:
        lpApplicationName: имя исполняемого файла
        lpCommandLine - содержит имя исполняемого файла и его аргумент (имя выходного файла)
        lpProcessAttributes: Указатель на структуру SECURITY_ATTRIBUTES
        lpThreadAttributes: Указатель на структуру SECURITY_ATTRIBUTES
        bInheritHandles: Устанавливается в TRUE чтобы дочерний процесс унаследовал все наследуемые дескрипторы родителя (p1_read, p2_read)
        dwCreationFlags: спецфлаги, например для нового консольного окна или приостановки процесса
        lpEnvironment: если NULL, дочерний процесс использует блок окружения родителя
        lpCurrentDirectory: если NULL, дочерний процесс использует рабочий каталог родителя
        lpStartupInfo: указатель на STARTUPINFOA, содержащую перенаправление stdin
        lpProcessInformation: указатель на PROCESS_INFORMATION, куда система запишет информацию о созданном процессе
    */
    if (!CreateProcessA(NULL, cmd1, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi1)) {
        CloseHandle(p1_read); CloseHandle(p1_write); CloseHandle(p2_read); CloseHandle(p2_write); return 1;
    }
    CloseHandle(p1_read); // parent больше не читает, чтобы корректно закрылся дочерний процесс при закрытии конца записи

    // child2: stdin <- p2_read
    ZeroMemory(&si, sizeof(si)); 
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = p2_read;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE); si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    char cmd2[BUF]; snprintf(cmd2, BUF, "child.exe \"%s\"", file2);
    if (!CreateProcessA(NULL, cmd2, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi2)) {
        CloseHandle(p1_write); CloseHandle(p2_read); CloseHandle(p2_write);
        CloseHandle(pi1.hProcess); CloseHandle(pi1.hThread);
        return 1;
    }
    CloseHandle(p2_read);

    // Основной цикл: читать строки и с вероятностью 80% писать в pipe1, иначе в pipe2
    srand((unsigned) GetTickCount());
    while (1) {
        int r = read_line(hStdin, line, BUF);
        if (r < 0) break; // EOF
        // дописать CRLF чтобы дочерний видел конец строки
        strcat(line, "\r\n");
        DWORD wrote;
        if ((rand() % 100) < 80) {
            WriteFile(p1_write, line, (DWORD)strlen(line), &wrote, NULL);
        } else {
            WriteFile(p2_write, line, (DWORD)strlen(line), &wrote, NULL);
        }
    }

    // Закрываем writer-ы — сигнал EOF дочерним процессам
    CloseHandle(p1_write); CloseHandle(p2_write);

    WaitForSingleObject(pi1.hProcess, INFINITE);
    WaitForSingleObject(pi2.hProcess, INFINITE);
    CloseHandle(pi1.hProcess); CloseHandle(pi1.hThread);
    CloseHandle(pi2.hProcess); CloseHandle(pi2.hThread);
    return 0;
}
