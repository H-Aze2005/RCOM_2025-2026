// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#define BUF_SIZE 256

#define FLAG 0x7E
#define A_TRANSMITTER 0x03
#define A_RECEIVER 0x01
#define SET 0x03
#define UA 0x07
#define DISC 0x0B

#define ROLE_TRANSMITTER 0
#define ROLE_RECEIVER 1

typedef enum
{
    Start,
    Flag_RCV,
    A_RCV,
    C_RCV,
    C_INF_RCV,
    BCC_RCV,
    BCC_INF_RCV,
    BCC_DATA,
    DATA_C,
    CTRL_T,
    DATA_S1,
    CTRL_S,
    CTRL_V,
    BCC2,
    BCC2_RCV,
    DATA_RCV,
    DATA_STUFF,
    Stop
} State;

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
LinkLayer globalConnectionParameters;

volatile int STOP = FALSE;

int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d received\n", alarmCount);
}

unsigned char CurrentPacket = 0x00;

int llopen(LinkLayer connectionParameters)
{

    globalConnectionParameters = connectionParameters;

    CurrentPacket = 0x00;

    alarmEnabled = FALSE;

    alarmCount = 0;

    const char *serialPort = connectionParameters.serialPort;

    State state = Start;

    if (openSerialPort(serialPort, connectionParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", serialPort);

    if (connectionParameters.role == ROLE_TRANSMITTER)
    {

        struct sigaction act = {0};
        act.sa_handler = &alarmHandler;
        if (sigaction(SIGALRM, &act, NULL) == -1)
        {
            perror("sigaction");
            return 1;
        }

        printf("Alarm configured\n");

        // Create SET frame
        unsigned char buf[1024] = {0};

        buf[0] = FLAG;
        buf[1] = A_TRANSMITTER;
        buf[2] = SET;
        buf[3] = A_TRANSMITTER ^ SET;
        buf[4] = FLAG;

        while (alarmCount < connectionParameters.nRetransmissions &&
               !alarmEnabled)
        {
            int bytes = writeBytesSerialPort(buf, 5);
            printf("Sending control word: ");
            int i = 0;

            while (i < 5)
            {
                printf("0x%02X ", buf[i]);
                i++;
            }
            printf("%d bytes written to serial port\n", bytes);
            sleep(1);
            if (alarmEnabled == FALSE)
            {
                alarm(connectionParameters.timeout);
                alarmEnabled = TRUE;
            }

            STOP = FALSE;
            unsigned char bufferRCV[BUF_SIZE] = {0};
            while (STOP == FALSE)
            {
                if (!alarmEnabled)
                {
                    break;
                }
                unsigned char byte;
                readByteSerialPort(&byte);

                switch (state)
                {
                case Start:
                    bufferRCV[0] = '\0';
                    if (byte == FLAG)
                    {
                        state = Flag_RCV;
                        bufferRCV[0] = FLAG;
                    }
                    else
                        state = Start;

                    break;

                case Flag_RCV:
                    if (byte == FLAG)
                        state = Flag_RCV;

                    else if (byte == A_TRANSMITTER)
                    {
                        state = A_RCV;
                        bufferRCV[1] = A_TRANSMITTER;
                    }

                    else
                        state = Start;

                    break;

                case A_RCV:
                    if (byte == FLAG)
                        state = Flag_RCV;

                    else if (byte == UA)
                    {
                        state = C_RCV;
                        bufferRCV[2] = UA;
                    }

                    else
                        state = Start;

                    break;

                case C_RCV:
                    if (byte == FLAG)
                        state = Flag_RCV;

                    else if (byte == (UA ^ A_TRANSMITTER))
                    {
                        bufferRCV[3] = UA ^ A_TRANSMITTER;
                        state = BCC_RCV;
                    }

                    else
                        state = Start;

                    break;

                case BCC_RCV:
                    if (byte == FLAG)
                    {
                        bufferRCV[4] = FLAG;
                        state = Stop;
                        STOP = TRUE;
                    }

                    else
                        state = Start;
                    break;

                case Stop:
                    STOP = TRUE;
                    break;
                default:
                    break;
                }
            }
            if (alarmEnabled)
            {
                printf("Received control word: ");
                alarmCount = 0;
                alarm(0);

                for (int i = 0; i < 5; i++)
                {
                    printf("0x%02X ", bufferRCV[i]);
                }
                break;
            }
        }

        return 0;
    }
    else
    {
        int nBytesBuf = 0;

        State state = Start;

        while (STOP == FALSE)
        {

            unsigned char byte;
            int bytes = readByteSerialPort(&byte);
            nBytesBuf += bytes;

            switch (state)
            {
            case Start:
                if (byte == FLAG)
                    state = Flag_RCV;

                else
                    state = Start;

                break;

            case Flag_RCV:
                if (byte == FLAG)
                    state = Flag_RCV;

                else if (byte == A_TRANSMITTER)
                    state = A_RCV;

                else
                    state = Start;

                break;

            case A_RCV:
                if (byte == FLAG)
                    state = Flag_RCV;

                else if (byte == SET)
                    state = C_RCV;

                else
                    state = Start;

                break;

            case C_RCV:
                if (byte == FLAG)
                    state = Flag_RCV;

                else if (byte == 0)
                    state = BCC_RCV;

                else
                    state = Start;

                break;

            case BCC_RCV:
                if (byte == FLAG)
                {
                    state = Stop;
                    STOP = TRUE;
                }

                else
                    state = Start;
                break;

            case Stop:
                STOP = TRUE;
                break;
            default:
                break;
            }
        }

        printf("transmitting");
        unsigned char buf[BUF_SIZE] = {0};

        buf[0] = FLAG;
        buf[1] = A_TRANSMITTER;
        buf[2] = UA;
        buf[3] = A_TRANSMITTER ^ UA;
        buf[4] = FLAG;

        sleep(1);
        writeBytesSerialPort(buf, 5);
        int i = 0;
        while (i < 5)
        {
            printf(" 0x%02X ", buf[i]);
            i++;
        }
        printf("\n");

        printf("Total bytes received: %d\n", nBytesBuf);

        return 0;
    }
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    alarmEnabled = FALSE;
    alarmCount = 0;

    int numBytesStuffed = 0;

    int REJ = TRUE;

    State state = Start;

    unsigned char InfFrame[bufSize * 2 + 6];
    memset(InfFrame, 0, bufSize * 2 + 6);

    InfFrame[0] = FLAG;
    InfFrame[1] = A_TRANSMITTER;
    InfFrame[2] = CurrentPacket;
    InfFrame[3] = A_TRANSMITTER ^ CurrentPacket;

    unsigned char BCC2 = 0x00;
    for (int i = 0; i < bufSize; i++)
    {
        BCC2 = BCC2 ^ buf[i];
        if (buf[i] == 0x7E)
        {
            InfFrame[4 + i + numBytesStuffed] = 0x7D;
            InfFrame[4 + i + 1 + numBytesStuffed] = 0x5E;
            numBytesStuffed++;
        }
        else if (buf[i] == 0x7D)
        {
            InfFrame[4 + i + numBytesStuffed] = 0x7D;
            InfFrame[4 + i + 1 + numBytesStuffed] = 0x5D;
            numBytesStuffed++;
        }
        else
        {
            InfFrame[4 + i + numBytesStuffed] = buf[i];
        }
    }

    InfFrame[4 + bufSize + numBytesStuffed] = BCC2;
    InfFrame[5 + bufSize + numBytesStuffed] = FLAG;

    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        return 1;
    }
    int bytes = 0;
    printf("Alarm configured\n");
    REJ = TRUE;
    while ((alarmCount < globalConnectionParameters.nRetransmissions && !alarmEnabled) || REJ)
    {
        bytes = writeBytesSerialPort(InfFrame, bufSize + 6 + numBytesStuffed);
        printf("Sending word: ");
        int i = 0;
        state = Start;
        while (i < 6 + bufSize + numBytesStuffed)
        {
            printf("0x%02X ", InfFrame[i]);
            i++;
        }
        printf("\n");

        sleep(0.1);
        if (alarmEnabled == FALSE)
        {
            alarm(globalConnectionParameters.timeout);
            alarmEnabled = TRUE;
        }

        STOP = FALSE;
        unsigned char bufferRCV[BUF_SIZE] = {0};
        while (STOP == FALSE)
        {
            if (!alarmEnabled)
            {
                break;
            }
            unsigned char byte;
            readByteSerialPort(&byte);

            switch (state)
            {
            case Start:
                bufferRCV[0] = '\0';
                if (byte == FLAG)
                {
                    state = Flag_RCV;
                    bufferRCV[0] = FLAG;
                }
                else
                    state = Start;

                break;
            case Flag_RCV:
                if (byte == FLAG)
                    state = Flag_RCV;

                else if (byte == A_TRANSMITTER)
                {
                    state = A_RCV;
                    bufferRCV[1] = A_TRANSMITTER;
                }
                else
                    state = Start;

                break;
            case A_RCV:
                if (byte == FLAG)
                    state = Flag_RCV;

                else if (byte == 0xAB)
                {
                    state = C_RCV;
                    CurrentPacket = 0x80;
                    REJ = FALSE;
                    bufferRCV[2] = byte;
                }
                else if (byte == 0xAA)
                {
                    state = C_RCV;
                    CurrentPacket = 0x00;
                    REJ = FALSE;
                    bufferRCV[2] = byte;
                }
                else if (byte == 0x55)
                {
                    REJ = TRUE;
                    state = C_RCV;
                    CurrentPacket = 0x80;
                    bufferRCV[2] = byte;
                }
                else if (byte == 0x54)
                {
                    REJ = TRUE;
                    state = C_RCV;
                    CurrentPacket = 0x00;
                    bufferRCV[2] = byte;
                }
                else
                    state = Start;

                break;
            case C_RCV:
                if (byte == FLAG)
                    state = Flag_RCV;

                else if (byte == (bufferRCV[1] ^ bufferRCV[2]))
                {
                    bufferRCV[3] = byte;
                    state = BCC_RCV;
                }
                else
                    state = Start;

                break;
            case BCC_RCV:
                if (byte == FLAG)
                {
                    bufferRCV[4] = FLAG;
                    writeBytesSerialPort(buf, 5);
                    state = Stop;
                    STOP = TRUE;
                }
                else
                    state = Start;
                break;

            case Stop:
                STOP = TRUE;
                break;
            default:
                break;
            }
        }
        if (alarmEnabled)
        {
            printf("Received control word: ");
            alarmCount = 0;
            alarm(0);

            for (int i = 0; i < 5; i++)
            {
                printf("0x%02X ", bufferRCV[i]);
            }
            if (bufferRCV[2] == 0x54 || bufferRCV[2] == 0x55)
            {
                printf("retrying: ");
                printf("%d \n", alarmEnabled);
                REJ = TRUE;
            }
            else
            {
                break;
            }
        }
    }

    return bytes;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    int nBytesBuf = 0;

    State state = Start;
    unsigned char control = 0;
    int packetBytes = 0;
    unsigned char bcc1 = 0;
    int dataSize = 0;
    int duplicate = 0;
    unsigned char bcc2 = 0x00;
    unsigned char result[2006];
    STOP = FALSE;

    while (STOP == FALSE)
    {

        unsigned char byte;
        int bytes = readByteSerialPort(&byte);
        nBytesBuf += bytes;

        switch (state)
        {
        case Start:
            if (byte == FLAG)
                state = Flag_RCV;
            else
                state = Start;

            break;
        case Flag_RCV:
            if (byte == FLAG)
                state = Flag_RCV;
            else if (byte == A_TRANSMITTER)
                state = A_RCV;
            else
                state = Start;
            break;
        case A_RCV:
            if (byte == FLAG)
                state = Flag_RCV;
            else if (byte == CurrentPacket)
            {
                CurrentPacket = CurrentPacket ^ 0x80;
                bcc1 = (byte ^ A_TRANSMITTER);
                state = BCC_DATA;
            }
            else if (byte == (CurrentPacket ^ 0x80))
            {
                state = BCC_DATA;
                bcc1 = (byte ^ A_TRANSMITTER);
                duplicate = 1;
            }
            else
            {
                control = byte;
                state = C_RCV;
            }
            break;
        case C_RCV:
            if (byte == FLAG)
                state = Flag_RCV;

            else if (byte == (A_TRANSMITTER ^ control))
                state = BCC_RCV;

            else
                state = Start;

            break;
        case BCC_DATA:
            if (byte == FLAG)
                state = Flag_RCV;

            else if (byte == bcc1)
            {
                state = DATA_RCV;
            }
            else
            {
                state = Start;
            }
            break;
        case BCC_RCV:
            if (control == SET)
            {
                if (byte == FLAG)
                {
                    state = Stop;
                    STOP = TRUE;

                    unsigned char buf[5] = {0};

                    buf[0] = FLAG;
                    buf[1] = A_TRANSMITTER;
                    buf[2] = UA;
                    buf[3] = A_TRANSMITTER ^ UA;
                    buf[4] = FLAG;

                    writeBytesSerialPort(buf, 5);
                }
                else
                    state = Start;
            }
            break;
        case BCC2:
            bcc2 = byte;
            state = BCC2_RCV;
            break;
        case BCC2_RCV:
            if (byte == FLAG)
            {
                state = STOP;
                STOP = TRUE;
            }
            else
            {
                state = Start;
            }
            break;
        case DATA_RCV:
            if (byte == FLAG)
            {
                state = Stop;
                STOP = TRUE;
            }
            else
            {
                result[dataSize] = byte;
                dataSize++;
                state = DATA_RCV;
            }
            break;
        case Stop:
            STOP = TRUE;
            break;
        default:
            break;
        }
    }

    unsigned char calculatedBCC2 = 0x00;
    bcc2 = result[dataSize - 1];
    printf(" 0x%02X\n ", bcc2);
    int i = 0;
    int j = 0;
    while (i < dataSize - 1)
    {
        if (result[i] == 0x7D)
        {
            if (result[i + 1] == 0x5D)
            {
                packet[j++] = 0x7D;
                i += 2;
            }
            else if (result[i + 1] == 0x5E)
            {
                packet[j++] = 0x7E;
                i += 2;
            }
            else
            {
                // Unexpected escape sequence
                packet[j++] = result[i++];
            }
        }
        else
        {
            packet[j++] = result[i++];
        }
    }
    packetBytes = j;

    for (int i = 0; i < packetBytes; i++)
    {
        calculatedBCC2 = calculatedBCC2 ^ packet[i];
        printf(" 0x%02X ", packet[i]);
    }
    if (bcc2 == calculatedBCC2)
    {
        unsigned char buf[5] = {0};
        buf[0] = FLAG;
        buf[1] = A_TRANSMITTER;
        buf[2] = 0xAA + (CurrentPacket == 0x80);
        buf[3] = A_TRANSMITTER ^ buf[2];
        buf[4] = FLAG;
        int i = 0;
        printf("transmitting: \n");
        while (i < 5)
        {
            printf(" 0x%02X ", buf[i]);
            i++;
        }

        writeBytesSerialPort(buf, 5);
    }
    else
    {
        CurrentPacket = (0x80 ^ CurrentPacket);
        unsigned char buf[5] = {0};
        buf[0] = FLAG;
        buf[1] = A_TRANSMITTER;
        buf[2] = 0x54 + (CurrentPacket == 0x80);
        buf[3] = A_TRANSMITTER ^ buf[2];
        buf[4] = FLAG;

        writeBytesSerialPort(buf, 5);
        printf("rejected \n");
        return -1;
    }
    printf("\n");

    if (duplicate)
    {
        return -1;
    }

    return packetBytes;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose()
{

    State state = Start;

    if (globalConnectionParameters.role == ROLE_RECEIVER)
    {

        alarmCount = 0;
        alarmEnabled = FALSE;
        struct sigaction act = {0};
        act.sa_handler = &alarmHandler;
        if (sigaction(SIGALRM, &act, NULL) == -1)
        {
            perror("sigaction");
            return 1;
        }

        printf("Alarm configured\n");

        // Create string to send
        unsigned char buf[1024] = {0};

        buf[0] = FLAG;
        buf[1] = A_TRANSMITTER;
        buf[2] = DISC;
        buf[3] = A_TRANSMITTER ^ DISC;
        buf[4] = FLAG;

        while (alarmCount < globalConnectionParameters.nRetransmissions && !alarmEnabled)
        {
            int bytes = writeBytesSerialPort(buf, 5);
            printf("Sending control word: ");
            int i = 0;

            while (i < 5)
            {
                printf("0x%02X ", buf[i]);
                i++;
            }
            printf("%d bytes written to serial port\n", bytes);
            sleep(1);
            if (alarmEnabled == FALSE)
            {
                alarm(globalConnectionParameters.timeout);
                alarmEnabled = TRUE;
            }

            STOP = FALSE;
            unsigned char bufferRCV[BUF_SIZE] = {0};
            while (STOP == FALSE)
            {
                if (!alarmEnabled)
                {
                    break;
                }
                unsigned char byte;
                readByteSerialPort(&byte);

                switch (state)
                {
                case Start:
                    bufferRCV[0] = '\0';
                    if (byte == FLAG)
                    {
                        state = Flag_RCV;
                        bufferRCV[0] = FLAG;
                    }
                    else
                        state = Start;
                    break;
                case Flag_RCV:
                    if (byte == FLAG)
                        state = Flag_RCV;
                    else if (byte == A_RECEIVER)
                    {
                        state = A_RCV;
                        bufferRCV[1] = A_RECEIVER;
                    }
                    else
                        state = Start;
                    break;
                case A_RCV:
                    if (byte == FLAG)
                        state = Flag_RCV;
                    else if (byte == DISC)
                    {
                        state = C_RCV;
                        bufferRCV[2] = DISC;
                    }
                    else
                        state = Start;
                    break;
                case C_RCV:
                    if (byte == FLAG)
                        state = Flag_RCV;
                    else if (byte == (bufferRCV[1] ^ bufferRCV[2]))
                    {
                        bufferRCV[3] = bufferRCV[1] ^ bufferRCV[2];
                        state = BCC_RCV;
                    }
                    else
                        state = Start;
                    break;
                case BCC_RCV:
                    if (byte == FLAG)
                    {
                        bufferRCV[4] = FLAG;
                        state = Stop;
                        STOP = TRUE;
                    }
                    else
                        state = Start;
                    break;
                case Stop:
                    STOP = TRUE;
                    break;
                default:
                    break;
                }
            }
            if (alarmEnabled)
            {
                printf("Received control word: ");
                alarmCount = 0;
                alarm(0);

                for (int i = 0; i < 5; i++)
                {
                    printf("0x%02X ", bufferRCV[i]);
                }
                if (bufferRCV[2] != DISC)
                {
                    printf("Did not receive disconnect byte\n");
                }
                else
                {
                    break;
                }
            }
        }

        unsigned char bufS[5] = {0};

        bufS[0] = FLAG;
        bufS[1] = A_RECEIVER;
        bufS[2] = UA;
        bufS[3] = A_RECEIVER ^ UA;
        bufS[4] = FLAG;
        sleep(0.2);
        writeBytesSerialPort(bufS, 5);

        printf("transmitting: ");
        for (int i = 0; i < 5; i++)
        {
            printf("0x%02X ", bufS[i]);
        }

        if (closeSerialPort() < 0)
        {
            perror("closeSerialPort");
        }
        printf("\nClosed the connection\n");
        return 0;
    }
    else
    {
        int nBytesBuf = 0;

        State state = Start;
        STOP = FALSE;
        while (STOP == FALSE)
        {

            unsigned char byte;
            int bytes = readByteSerialPort(&byte);
            nBytesBuf += bytes;

            switch (state)
            {
            case Start:
                if (byte == FLAG)
                    state = Flag_RCV;
                else
                    state = Start;
                break;
            case Flag_RCV:
                if (byte == FLAG)
                    state = Flag_RCV;
                else if (byte == A_TRANSMITTER)
                    state = A_RCV;
                else
                    state = Start;
                break;
            case A_RCV:
                if (byte == FLAG)
                    state = Flag_RCV;
                else if (byte == DISC)
                    state = C_RCV;
                else
                    state = Start;
                break;
            case C_RCV:
                if (byte == FLAG)
                    state = Flag_RCV;
                else if (byte == (DISC ^ A_TRANSMITTER))
                    state = BCC_RCV;
                else
                    state = Start;
                break;
            case BCC_RCV:
                if (byte == FLAG)
                {
                    state = Stop;
                    STOP = TRUE;
                }
                else
                    state = Start;
                break;
            case Stop:
                STOP = TRUE;
                break;
            default:
                break;
            }
        }
        unsigned char buf[BUF_SIZE] = {0};

        buf[0] = FLAG;
        buf[1] = A_RECEIVER;
        buf[2] = DISC;
        buf[3] = A_RECEIVER ^ DISC;
        buf[4] = FLAG;

        state = Start;
        struct sigaction act = {0};
        act.sa_handler = &alarmHandler;
        if (sigaction(SIGALRM, &act, NULL) == -1)
        {
            perror("sigaction");
            return 1;
        }

        printf("Alarm configured\n");

        alarmCount = 0;
        alarmEnabled = FALSE;
        while (alarmCount < globalConnectionParameters.nRetransmissions && !alarmEnabled)
        {
            int bytes = writeBytesSerialPort(buf, 5);
            printf("Sending control word: ");
            int i = 0;

            while (i < 5)
            {
                printf("0x%02X ", buf[i]);
                i++;
            }
            printf("%d bytes written to serial port\n", bytes);
            if (alarmEnabled == FALSE)
            {
                alarm(globalConnectionParameters.timeout);
                alarmEnabled = TRUE;
            }

            STOP = FALSE;
            unsigned char bufferRCV[BUF_SIZE] = {0};
            while (STOP == FALSE)
            {
                if (!alarmEnabled)
                {
                    break;
                }
                unsigned char byte;
                readByteSerialPort(&byte);

                switch (state)
                {
                case Start:
                    printf("start\n");
                    bufferRCV[0] = '\0';
                    if (byte == FLAG)
                    {
                        state = Flag_RCV;
                        bufferRCV[0] = FLAG;
                    }
                    else
                        state = Start;
                    break;
                case Flag_RCV:
                    if (byte == FLAG)
                        state = Flag_RCV;
                    else if (byte == A_RECEIVER)
                    {
                        state = A_RCV;
                        bufferRCV[1] = A_RECEIVER;
                    }
                    else
                        state = Start;
                    break;
                case A_RCV:
                    if (byte == FLAG)
                        state = Flag_RCV;
                    else if (byte == UA)
                    {
                        state = C_RCV;
                        bufferRCV[2] = UA;
                    }
                    else
                        state = Start;
                    break;
                case C_RCV:
                    if (byte == FLAG)
                        state = Flag_RCV;

                    else if (byte == (UA ^ A_RECEIVER))
                    {
                        bufferRCV[3] = UA ^ A_RECEIVER;
                        state = BCC_RCV;
                    }
                    else
                        state = Start;
                    break;
                case BCC_RCV:
                    if (byte == FLAG)
                    {
                        bufferRCV[4] = FLAG;
                        state = Stop;
                        STOP = TRUE;
                    }
                    else
                        state = Start;
                    break;
                case Stop:
                    STOP = TRUE;
                    break;
                default:
                    break;
                }
            }
            if (alarmEnabled)
            {
                printf("Received control word: ");
                alarmCount = 0;
                alarm(0);

                for (int i = 0; i < 5; i++)
                {
                    printf("0x%02X ", bufferRCV[i]);
                }
                break;
            }
        }
        if (closeSerialPort() < 0)
        {
            perror("closeSerialPort");
        }
        printf("\nClosed the connection\n");
        return 0;
    }
}
