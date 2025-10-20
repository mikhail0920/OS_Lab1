#define _CRT_SECURE_NO_WARNINGS //отключить предупреждения о низкоуровневых функциях
#include <windows.h> //WinAPI

#define BUF 1024

static int read_line(HANDLE hStdin, char *out, int maxlen) {
    DWORD n; char c; int pos = 0;
    while (1) {
        //если n==0, конец "файла", значит конец pipe закрыт
        //дескриптор источника чтения; &c указатель на буфер, куда записываем; 1 - кол-во байтов на чтение; &n указатель на DWORD, куда записывается фактическое кол-во прочитанных байт; NULL - нужно при асинхронном выводе
        if (!ReadFile(hStdin, &c, 1, &n, NULL) || n == 0) return -1;
        if (c == '\r') continue; //пропуск символа возврата каретки
        if (c == '\n') break; //строка прочитана, выходми
        if (pos < maxlen-1) out[pos++] = c;
    }
    out[pos] = 0; //делаем строку Си-валидной (null-terminator)
    return pos;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1; //должно быть передано название файла
    //argv[1] имя файла от род процесса; GENERIC_WRITE для записи; 0 - dwShareMode, запрет совместного доступа; NULL - указатель на структуру SECURITY_ATRIBUTES; CREATE_ALWAYS - всегда создаём/перезаписываем файл; FILE_ATTRIBUTE_NORMAL - обычные атрибуты файла
    HANDLE hf = CreateFileA(argv[1], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hf == INVALID_HANDLE_VALUE) return 1;

    //получаем дескриптор стандартного ввода, он указывает на конец pipe
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    char line[BUF];
    while (1) {
        int len = read_line(hStdin, line, BUF);
        if (len < 0) break; //канал закрыт род.процессом
        // инвертируем
        for (int i = 0, j = len-1; i < j; ++i, --j) { char t = line[i]; line[i]=line[j]; line[j]=t; }
        line[len] = '\r'; line[len+1] = '\n';
        DWORD written;
        //hf дескриптор выхода; line буфер (строка); (DWORD)(len+2) кол-во байтов для записи; &written кол-во реально записанных байтов; NULL-асинхронка
        WriteFile(hf, line, (DWORD)(len+2), &written, NULL);
    }

    CloseHandle(hf);
    return 0;
}
