#include <windows.h>
#include <stdio.h>

#define PACKET_SIZE 110
#define SYNC_PATTERN_SIZE 4
#define DATA_SIZE 104
#define TIMEOUT_INTERVAL 100 // Timpul de așteptare în milisecunde
#define TARGET_INDEX_MIN 45  // Indexul minim
#define TARGET_INDEX_MAX 49  // Indexul maxim

// Funcție pentru a obține timpul sistemului sub formă de șir de caractere
char* getCurrentTime() {
    SYSTEMTIME time;
    GetLocalTime(&time);
    static char timeString[20];
    sprintf(timeString, "%02d:%02d:%02d.%03d", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
    return timeString;
}

int main() {
    // Deschiderea portului serial COM3
    HANDLE hSerial = CreateFile("COM3",
                                GENERIC_READ, // deschidem portul pentru citire
                                0,            // comunicație exclusivă, fără share
                                NULL,         // fără securitate
                                OPEN_EXISTING,// deschidem portul existent
                                0,            // fără atribute speciale
                                NULL);        // fără template

    if (hSerial == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            printf("Portul serial nu a fost gasit.\n");
        }
        printf("Nu s-a putut deschide portul serial.\n");
        return 1;
    }

    // Configurarea parametrilor portului serial
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        printf("Eroare la obținerea starii portului serial.\n");
        CloseHandle(hSerial);
        return 1;
    }

    dcbSerialParams.BaudRate = CBR_2400; // Viteza de 2400 baud
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity   = NOPARITY;
    if (!SetCommState(hSerial, &dcbSerialParams)) {
        printf("Eroare la configurarea portului serial.\n");
        CloseHandle(hSerial);
        return 1;
    }

    // Setarea timeout-ului pentru citirea de pe portul serial
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = TIMEOUT_INTERVAL;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    if (!SetCommTimeouts(hSerial, &timeouts)) {
        printf("Eroare la setarea timeout-ului pentru citire.\n");
        CloseHandle(hSerial);
        return 1;
    }

    // Declarație și inițializare array DATE pentru stocarea celor 104 octeți
    unsigned char DATE[DATA_SIZE];

    // Deschiderea fișierului CSV pentru scriere
    FILE *csvFile = fopen("output.csv", "w");
    if (csvFile == NULL) {
        printf("Eroare la deschiderea fișierului CSV.\n");
        CloseHandle(hSerial);
        return 1;
    }

    // Scrierea antetului coloanelor în fișierul CSV
    fprintf(csvFile, "Timp sistem,octeti specifici (Wh)\n");

    // Bucla infinită pentru citirea continuă
    while (1) {
        // Așteptarea unui caracter pe portul serial
        unsigned char szBuff[PACKET_SIZE] = {0};
        DWORD dwBytesRead = 0;
        if (!ReadFile(hSerial, szBuff, PACKET_SIZE, &dwBytesRead, NULL)) {
            if (GetLastError() == ERROR_TIMEOUT) {
                printf("Nu s-a primit niciun caracter în ultimele %d ms.\n", TIMEOUT_INTERVAL);
            } else {
                printf("Eroare la citirea din portul serial.\n");
            }
            continue;
        }

        // Verificarea sincronizării și salvarea datelor în array-ul DATE
        int syncIndex = -1;
        for (int i = 0; i < PACKET_SIZE - SYNC_PATTERN_SIZE; ++i) {
            if (memcmp(szBuff + i, "\x01\x00\x68\x02", 4) == 0) {
                syncIndex = i;
                printf("Sincronizare gasita la indexul %d.\n", syncIndex);

                // Copierea celor 104 octeți în array-ul DATE
                for (int j = 0; j < DATA_SIZE; ++j) {
                    DATE[j] = szBuff[syncIndex + SYNC_PATTERN_SIZE + j];
                }

                // Scrierea timpului sistemului și octeților în fișierul CSV
                fprintf(csvFile, "%s,", getCurrentTime());
                for (int j = TARGET_INDEX_MIN; j <= TARGET_INDEX_MAX; ++j) {
                    fprintf(csvFile, "%02X", DATE[j]);
                }
                fprintf(csvFile, " Wh\n");

                // Afișarea octeților specifici din array-ul DATE
                printf("Octetii specifici din array-ul DATE: ");

                // Afisarea octetilor concatenati
                for (int j = TARGET_INDEX_MIN; j <= TARGET_INDEX_MAX; ++j) {
                    printf("%02X", DATE[j]);
                }
                printf(" Wh\n");

                // Afișarea octetului ETX
                printf("Octetul ETX: %02X\n", szBuff[syncIndex + PACKET_SIZE - 2]);

                // Afișarea octetului BCC (checksum-ul)
                printf("Octetul BCC (checksum-ul): %02X\n", szBuff[syncIndex + PACKET_SIZE - 1]);

                break;
            }
        }
    }

    // Închiderea fișierului CSV și a portului serial - acest cod nu va fi niciodată executat în bucla infinită
    fclose(csvFile);
    CloseHandle(hSerial);
    return 0;
}
