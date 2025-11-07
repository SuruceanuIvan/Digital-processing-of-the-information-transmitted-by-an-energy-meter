# Digital-processing-of-the-information-transmitted-by-an-energy-meter

![contor_prezentare](https://github.com/user-attachments/assets/453baf43-e5a9-4edc-80df-8089dbd5e520)

Worked in a team to develop a <b>system for reading energy pulses from electric meters</b>, integrating hardware and software components. Focused on the <b>software side</b>, writing a <b>PC application in C</b> using the <b>Windows API</b> for serial communication. The program handled <b>real-time pulse detection, timing measurement, and data logging.</b>

The hardware used a <b>custom circuit with an optical sensor</b> and <b>USB-to-serial interface (FT232RL)</b> to transmit data. I coordinated with hardware developers to ensure reliable signal acquisition and communication. The final tool was successfully implemented for <b>university research</b> on energy monitoring, offering accurate real-time data logging.

This project strengthened my skills in <b>low-level programming, system integration,</b> and <b>team collaboration</b> within a multidisciplinary engineering environment.

## Electronic schematic
<img width="1246" height="690" alt="schema_electronica" src="https://github.com/user-attachments/assets/879b12ca-53fc-49e4-b2a5-7b002c54d28f" />

## Code Overview

## Constant Definitions

This block defines key constants used throughout the program to ensure consistent configuration and easy maintenance of the data processing logic:

```cpp
#define PACKET_SIZE 110
#define SYNC_PATTERN_SIZE 4
#define DATA_SIZE 104
#define TIMEOUT_INTERVAL 100 
#define TARGET_INDEX_MIN 45 
#define TARGET_INDEX_MAX 49
```
<ul>
<li><b>PACKET_SIZE (110)</b> – Defines the total number of bytes expected in each data packet received from the energy meter via the serial interface. This helps allocate the correct buffer size and ensures complete data acquisition.</li>

<li><b>SYNC_PATTERN_SIZE (4)</b> – Specifies the number of bytes used for the synchronization pattern (0x01 0x00 0x68 0x02). The program uses this pattern to identify the beginning of a valid data frame within the incoming byte stream.</li>

<li><b>DATA_SIZE (104)</b> – Represents the number of data bytes that follow the synchronization sequence. These bytes contain the actual measurement and status information from the energy meter.</li>

<li><b>TIMEOUT_INTERVAL (100)</b> – Defines the read timeout in milliseconds. This prevents the system from blocking indefinitely while waiting for data, improving reliability and responsiveness of the serial communication.</li>

<li><b>TARGET_INDEX_MIN (45)</b> and <b>TARGET_INDEX_MAX (49)</b> – Define the range of specific bytes within the data frame that correspond to the energy consumption value (in Wh). These indices are used to extract and display the relevant portion of the data both on-screen and in the CSV output file.</li>
</ul>

## Function: getCurrentTime()

```cpp
char* getCurrentTime() {
    SYSTEMTIME time;
    GetLocalTime(&time);
    static char timeString[20];
    sprintf(timeString, "%02d:%02d:%02d.%03d", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
    return timeString;
}
```

<p>This function retrieves the <b>current system time</b> and formats it into a human-readable string that includes hours, minutes, seconds, and milliseconds. It is used to timestamp data entries, allowing precise tracking of when each data packet was received from the energy meter.</p>

<p>In the main program, this function is called each time a valid data packet is received. The returned timestamp is written into the <b>CSV file</b>, allowing every energy measurement to be logged with precise timing information — an essential feature for performance analysis and diagnostic tracking.</p>

## Serial Port Initialization — Opening COM3

```cpp
    // Opening serial port COM3
    HANDLE hSerial = CreateFile("COM3",
                                GENERIC_READ, // open port for reading
                                0,            // just communication without share
                                NULL,         // no security
                                OPEN_EXISTING,// open existing port
                                0,            // without special attributes
                                NULL);        // without template
```
This section of code is responsible for <b>opening and validating</b> communication with the serial port (COM3). It establishes a handle to the port, which will later be used to read incoming data from the energy meter.

```cpp
    if (hSerial == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            printf("Serial port not found.\n");
        }
        printf("Can't open serial port.\n");
        return 1;
    }
```

This block is the <b>first step in establishing communication</b> with the energy meter. By successfully obtaining a valid handle to COM3, the system ensures that the following stages — configuring the baud rate, setting data format, and reading incoming frames — can proceed without hardware-level errors.

## Serial Port Configuration — Communication Parameters Setup

```cpp
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        printf("Error at getting port state.\n");
        CloseHandle(hSerial);
        return 1;
    }

    dcbSerialParams.BaudRate = CBR_2400; // Speed of 2400 baud
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity   = NOPARITY;
    if (!SetCommState(hSerial, &dcbSerialParams)) {
        printf("Error at configuration of the port.\n");
        CloseHandle(hSerial);
        return 1;
    }
```

This section of the program defines and applies the <b>communication parameters</b> for the serial connection with the energy meter. It ensures both the PC and the microcontroller (or energy measurement device) communicate using the same data format and timing.

This configuration block ensures that the <b>communication link between the PC and the energy meter</b> is properly <b>synchronized</b> in terms of data speed and format. It defines the physical layer parameters required before initiating data transfer, guaranteeing reliable serial communication and accurate frame interpretation.

## Serial Port Timeout Configuration

```cpp
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = TIMEOUT_INTERVAL;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    if (!SetCommTimeouts(hSerial, &timeouts)) {
        printf("Error at setting timeout for reading.\n");
        CloseHandle(hSerial);
        return 1;
    }
```

This block configures the <b>read and write timeout behavior</b> of the serial port, preventing the program from blocking indefinitely if data is not immediately available. Proper timeout handling is critical for <b>real-time data acquisition</b> from the energy meter.

By configuring timeouts, this section ensures <b>robust serial communication</b>. The program can handle <b>delays or temporary absence of data</b> from the energy meter without hanging indefinitely, which is critical for continuous monitoring and real-time data logging to the CSV file.

## Continuous Data Reception and Synchronization Loop

```cpp
    // Infinite loop for reading data
    while (1) {
        // Waiting for one character at serial port
        unsigned char szBuff[PACKET_SIZE] = {0};
        DWORD dwBytesRead = 0;
        if (!ReadFile(hSerial, szBuff, PACKET_SIZE, &dwBytesRead, NULL)) {
            if (GetLastError() == ERROR_TIMEOUT) {
                printf("Don't get the character in %d ms.\n", TIMEOUT_INTERVAL);
            } else {
                printf("Error at reading from serial port.\n");
            }
            continue;
        }

        // Verify syncronization and saving data in array DATE
        int syncIndex = -1;
        for (int i = 0; i < PACKET_SIZE - SYNC_PATTERN_SIZE; ++i) {
            if (memcmp(szBuff + i, "\x01\x00\x68\x02", 4) == 0) {
                syncIndex = i;
                printf("Find syncronization at index %d.\n", syncIndex);

                // Copy 104 bytes in array DATE
                for (int j = 0; j < DATA_SIZE; ++j) {
                    DATE[j] = szBuff[syncIndex + SYNC_PATTERN_SIZE + j];
                }

                // Writing system time and bytes in CSV file
                fprintf(csvFile, "%s,", getCurrentTime());
                for (int j = TARGET_INDEX_MIN; j <= TARGET_INDEX_MAX; ++j) {
                    fprintf(csvFile, "%02X", DATE[j]);
                }
                fprintf(csvFile, " Wh\n");

                // Showing specific bytes from array DATE
                printf("Specific bytes from DATE: ");

                // Showing concatenated bytes
                for (int j = TARGET_INDEX_MIN; j <= TARGET_INDEX_MAX; ++j) {
                    printf("%02X", DATE[j]);
                }
                printf(" Wh\n");

                // Showing ETX byte
                printf("Byte ETX: %02X\n", szBuff[syncIndex + PACKET_SIZE - 2]);

                // Showing byte BCC (checksum)
                printf("Byte BCC (checksum-): %02X\n", szBuff[syncIndex + PACKET_SIZE - 1]);

                break;
            }
        }
    }
```

This block implements the <b>main loop for continuous data acquisition</b> from the energy meter over the serial port. It handles reading, synchronization, parsing, logging, and displaying relevant data in real time.

<ul>
<li>Reading from the serial port (ReadFile)</li>

<ul>
<li>Allocates a buffer szBuff of size PACKET_SIZE to store the incoming packet.</li>

<li>ReadFile() attempts to read up to PACKET_SIZE bytes.</li>

<li>If no data arrives within the timeout, it prints a warning and continues waiting.</li>

<li>Any other errors are also reported, ensuring robustness in communication.</li>

</ul>

<li>Synchronization detection</li>

<ul>

<li>Uses a sliding window to search for the 4-byte synchronization pattern 0x01 0x00 0x68 0x02.</li>

<li>memcmp() compares consecutive bytes in the buffer until the pattern is found.</li>

<li>syncIndex marks the start of a valid data frame.</li>

</ul>

<li>Data extraction</li>

<ul>

<li>After finding the sync pattern, the program copies the following 104 data bytes into the DATE array.</li>

<li>This isolates the measurement and status data for further processing.</li>

</ul>

<li>CSV Logging</li>

<ul>

<li>Writes the system timestamp using getCurrentTime().</li>

<li>Extracts and logs the energy consumption bytes (indices TARGET_INDEX_MIN to TARGET_INDEX_MAX) in hexadecimal format.</li>

<li>Appends a "Wh" suffix for clarity.</li>

</ul>

<li>Console Output</li>

<ul>

<li>Displays the same specific data bytes, making the program useful for real-time monitoring and debugging.</li>

<li>Prints the ETX byte (end-of-text marker) and BCC byte (checksum), which are important for verifying the integrity of the received packet.</li>

</ul>
</ul>

This loop forms the core of the energy meter interface:

<ul>

<li>It ensures that every packet is validated (via sync pattern), parsed correctly, and logged with accurate timestamps.</li>

<li>By continuously extracting, displaying, and saving data, it allows for real-time energy monitoring and later analysis through the CSV file.</li>

</ul>
