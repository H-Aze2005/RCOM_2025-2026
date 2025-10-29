// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define FALSE 0
#define TRUE 1

#define BAUDRATE 38400
#define BUF_SIZE 6

int alarmEnabled = TRUE;
int alarmCount = 0;

// Alarm function handler.
// This function will run whenever the signal SIGALRM is received.
void alarmHandler(int signal)
{
    // Can be used to change a flag that increases the number of alarms
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d received\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////

//only the flag, A and C are different in UA
//What is received from UA
#define FLAG_RCV 0x7E
#define A_RCV 0x01 //received from UA
#define C_RCV 0x07 //received from UA
#define A_TRANSMITTER 0x03
#define C_SET 0x03
#define C_UA 0x07
#define C_DISC 0x0B
#define C_I0 0x00
#define C_I1 0x80
#define ESC 0x7D

unsigned char FLAG = 0x7E;
unsigned char A_EMISSOR = 0x03;

typedef enum{
    UA_START_STATE,
    UA_FLAG_RCV_STATE,
    UA_A_RCV_STATE,
    UA_C_RCV_STATE,
    UA_BCC_OK_STATE,
    UA_STOP_STATE
} UAStateMachine;

typedef enum{
    SET_START_STATE,
    SET_FLAG_RCV_STATE,
    SET_A_RCV_STATE,
    SET_C_RCV_STATE,
    SET_BCC_OK_STATE,
    SET_STOP_STATE
} SETStateMachine;

typedef enum {
    START,        // Initial state, waiting for the FLAG byte
    FLAG_RECEIVED,     // FLAG byte received
    A_RECEIVED,        // Address field received
    C_RECEIVED,        // Control field received
    BCC_OK,       // BCC1 (Address ^ Control) verified
    DATA,         // Receiving data bytes
    STOP_STATE    // End of frame detected (final FLAG byte)
} StateMachine;

int fd = -1;           // File descriptor for open serial port
struct termios oldtio; // Serial port settings to restore on closing
volatile int STOP = FALSE;

int llopen(LinkLayer connectionParameters)
{
    
    const char *serialPort = connectionParameters.serialPort;

    if (openSerialPort(serialPort, BAUDRATE) < 0)
    {
        perror("openSerialPort");
        exit(-1);
    }

    printf("Serial port %s opened\n", serialPort);

    // Set alarm function handler.
    // Install the function signal to be automatically invoked when the timer expires,
    // invoking in its turn the user function alarmHandler
    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    //activate alarm
    int t = 3;
    alarm(t);

    if(connectionParameters.role == LlTx){ //if it is a transmiter
        //created main string to send
        unsigned char buf_set[5];
        buf_set[0] = FLAG;
        buf_set[1] = A_EMISSOR;
        buf_set[2] = C_SET;
        buf_set[3] = (A_EMISSOR ^ C_SET);
        buf_set[4] = FLAG;

        int bytes = writeBytesSerialPort(buf_set, BUF_SIZE);
        printf("%d bytes written to serial port\n", bytes);

        // Wait until all bytes have been written to the serial port
        sleep(1);


        // Read Aknowledgement
        int nBytesBuf = 0;

        int state = UA_START_STATE; //starting state
        while (state != UA_STOP_STATE && alarmCount < 3)
    {        
        //printf("Entrou while loop\n");
        // Read one byte from serial port.
        // NOTE: You must check how many bytes were actually read by reading the return value.
        // In this example, we assume that the byte is always read, which may not be true.
        unsigned char byte;
        int bytes = readByteSerialPort(&byte);
        nBytesBuf += bytes;

        switch (state)
        {
        case UA_START_STATE:
            /* code */
            if (byte == FLAG_RCV){
                state = UA_FLAG_RCV_STATE;
            } else{
                state = UA_START_STATE;
            }
            break;
        case UA_FLAG_RCV_STATE:
            if(byte == FLAG_RCV){
                state = UA_FLAG_RCV_STATE;
            } else if (byte == A_RCV){
                state = UA_A_RCV_STATE;
            } else {

                state = UA_START_STATE;
            }
            break;
        case UA_A_RCV_STATE:
            if(byte == FLAG_RCV){
                state = UA_FLAG_RCV_STATE;
            } else if (byte == C_RCV){
                state = UA_C_RCV_STATE;
            } else {
                state = UA_START_STATE;
            }
            break;
        case UA_C_RCV_STATE:
            if(byte == FLAG_RCV){
                state = UA_FLAG_RCV_STATE;
            } else if (byte == (A_RCV ^ C_RCV)){
                state = UA_BCC_OK_STATE;
            } else {
                state = UA_START_STATE;
            }
            break;
        case UA_BCC_OK_STATE:
            if(byte == FLAG_RCV){
                state = UA_STOP_STATE;
            } else {
                state = UA_START_STATE;
            }
            break;
        case UA_STOP_STATE:
            alarm(0); //deactivates alarm
            break;
        }
    }
        
    } else if (connectionParameters.role == LlRx) {//receiver
        int state = SET_START_STATE; // Starting state for SET frame
        unsigned char byte;

        while (state != SET_STOP_STATE) {
            // Read one byte from the serial port
            if (readByteSerialPort(&byte) <= 0) {
                continue; // No byte received, try again
            }

            switch (state) {
                case SET_START_STATE:
                    if (byte == FLAG_RCV) {
                        state = SET_FLAG_RCV_STATE;
                    }
                    break;

                case SET_FLAG_RCV_STATE:
                    if (byte == A_TRANSMITTER) {
                        state = SET_A_RCV_STATE;
                    } else if (byte != FLAG_RCV) {
                        state = SET_START_STATE; // Reset state if unexpected byte
                    }
                    break;

                case SET_A_RCV_STATE:
                    if (byte == C_SET) {
                        state = SET_C_RCV_STATE;
                    } else if (byte == FLAG_RCV) {
                        state = SET_FLAG_RCV_STATE; // Go back to FLAG state
                    } else {
                        state = SET_START_STATE; // Reset state if unexpected byte
                    }
                    break;

                case SET_C_RCV_STATE:
                    if (byte == (A_TRANSMITTER ^ C_SET)) { // Check BCC1
                        state = SET_BCC_OK_STATE;
                    } else if (byte == FLAG_RCV) {
                        state = SET_FLAG_RCV_STATE; // Go back to FLAG state
                    } else {
                        state = SET_START_STATE; // Reset state if BCC1 is incorrect
                    }
                    break;

                case SET_BCC_OK_STATE:
                    if (byte == FLAG_RCV) {
                        state = SET_STOP_STATE; // End of frame detected
                    } else {
                        state = SET_START_STATE; // Reset state if unexpected byte
                    }
                    break;

                case SET_STOP_STATE:
                    // Should not reach here
                    break;
            }
        }

        // Send UA frame as a response
        unsigned char uaFrame[5];
        uaFrame[0] = FLAG;
        uaFrame[1] = A_RCV; // Address field for receiver
        uaFrame[2] = C_UA;  // Control field for UA
        uaFrame[3] = (A_RCV ^ C_UA); // BCC1
        uaFrame[4] = FLAG;

        int bytesWritten = writeBytesSerialPort(uaFrame, sizeof(uaFrame));
        if (bytesWritten != sizeof(uaFrame)) {
            perror("Error sending UA frame");
            return -1;
        }

        printf("UA frame sent successfully\n");
    } else {
        fprintf(stderr, "Invalid role specified\n");
        return -1;
    }

    return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    static unsigned char sequenceNum = 0;
    // will use this buf as data bytes in out Information Frame
    // we need to build the Information packet
    unsigned char InfFrame[6 + bufSize];
    InfFrame[0] = FLAG_RCV; //first flag
    InfFrame[1] = A_EMISSOR;
    InfFrame[2] = (sequenceNum << 7);// 0x00 for I(0) and 0x80 for I(1)
    InfFrame[3] = (A_EMISSOR ^ sequenceNum);

    unsigned char BCC2 = 0x00;
    
    for(int i = 4; i <= 3 + bufSize; i++){
        InfFrame[i] = buf[i - 4];
        BCC2 ^= InfFrame[i];
    }

    InfFrame[4 + bufSize] = BCC2;
    InfFrame[5 + bufSize] = FLAG_RCV; 
    

    // Byte Stuffing
    // if we see Flag inside data then it is replaced by ESC 0x5e, 0x5e is 0X7E ^ 0x20
    // if we find ESC inside data then it is replaced by ESC 0x5d, 0x5d id 0x7D ^ 0x20
    unsigned char newBuf[2 * (6 + bufSize)]; //new buffer  needs to be created to store the new octets from the byte stuffing
    int stuffedIndex = 0;

    newBuf[stuffedIndex++] = InfFrame[0];

    for (int i = 4; i <= (6 + bufSize) - 2; i++){ //iterate over the new buffer, goes from D1 to BCC2 (inclusive)
        if(InfFrame[i] == FLAG_RCV){
            newBuf[stuffedIndex++] = ESC;
            newBuf[stuffedIndex++] = ESC ^ InfFrame[i];
        } else if (InfFrame[i] == ESC){
            newBuf[stuffedIndex++] = ESC;
            newBuf[stuffedIndex++] = ESC ^ InfFrame[i];
        } else {
            newBuf[stuffedIndex++] = InfFrame[i];
        }
    }
    newBuf[stuffedIndex++] = FLAG_RCV;

    int bytes = writeBytesSerialPort(newBuf, 2*(6 + bufSize));
        printf("%d bytes written to serial port\n", bytes);
    
    // Wait until all bytes have been written to the serial port
    sleep(1);

    sequenceNum ^= 1; //alternate between 0x00 and 0x80
    

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
    StateMachine state = START;
    unsigned char byte;
    unsigned char addressField = 0;
    unsigned char controlField = 0;
    unsigned char bcc1 = 0;
    unsigned char bcc2 = 0;
    unsigned char calculatedBcc2 = 0;
    int dataIndex = 0;

    while (state != STOP_STATE) {
        // Read one byte from the serial port
        if (readByteSerialPort(&byte) <= 0) {
            continue; // No byte received, try again
        }

        switch (state) {
            case START:
                if (byte == FLAG_RCV) {
                    state = FLAG_RECEIVED;
                }
                break;

            case FLAG_RECEIVED:
                if (byte == A_TRANSMITTER || byte == A_RCV) {
                    addressField = byte;
                    state = A_RECEIVED;
                } else if (byte == FLAG_RCV) {
                    // Stay in FLAG_RECEIVED state
                } else {
                    state = START;
                }
                break;

            case A_RECEIVED:
                if (byte == C_I0 || byte == C_I1) {
                    controlField = byte;
                    bcc1 = addressField ^ controlField;
                    state = C_RECEIVED;
                } else if (byte == FLAG_RCV) {
                    state = FLAG_RECEIVED;
                } else {
                    state = START;
                }
                break;

            case C_RECEIVED:
                if (byte == bcc1) {
                    state = BCC_OK;
                } else if (byte == FLAG_RCV) {
                    state = FLAG_RECEIVED;
                } else {
                    state = START;
                }
                break;

            case BCC_OK:
                if (byte == FLAG_RCV) {
                    // No data, supervision frame
                    return 0;
                } else {
                    // Start of data
                    if (byte == ESC) {
                        // Read next byte for destuffing
                        if (readByteSerialPort(&byte) <= 0) {
                            state = START;
                            continue;
                        }
                        byte ^= 0x20; // Destuff
                    }
                    packet[dataIndex++] = byte;
                    calculatedBcc2 ^= byte;
                    state = DATA;
                }
                break;

            case DATA:
                if (byte == FLAG_RCV) {
                    // End of frame
                    if (dataIndex > 0) {
                        // Last byte should be BCC2
                        bcc2 = packet[dataIndex - 1];
                        dataIndex--; // Remove BCC2 from data
                        calculatedBcc2 ^= bcc2; // Remove BCC2 from calculation

                        if (calculatedBcc2 == 0) {
                            // BCC2 is correct
                            return dataIndex; // Return the size of the payload
                        } else {
                            // BCC2 error, reset
                            state = START;
                            dataIndex = 0;
                            calculatedBcc2 = 0;
                        }
                    } else {
                        state = START;
                    }
                } else {
                    // Continue reading data
                    if (byte == ESC) {
                        // Read next byte for destuffing
                        if (readByteSerialPort(&byte) <= 0) {
                            state = START;
                            dataIndex = 0;
                            calculatedBcc2 = 0;
                            continue;
                        }
                        byte ^= 0x20; // Destuff
                    }
                    if (dataIndex < MAX_PAYLOAD_SIZE) {
                        packet[dataIndex++] = byte;
                        calculatedBcc2 ^= byte;
                    } else {
                        // Packet too large, reset
                        state = START;
                        dataIndex = 0;
                        calculatedBcc2 = 0;
                    }
                }
                break;

            case STOP_STATE:
                // Should not reach here
                break;
        }
    }

    return -1; // Error
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(LinkLayer connectionParameters)
{
    if (connectionParameters.role == LlTx) { // Transmitter
        // Set alarm function handler
        struct sigaction act = {0};
        act.sa_handler = &alarmHandler;
        if (sigaction(SIGALRM, &act, NULL) == -1)
        {
            perror("sigaction");
            return -1;
        }

        // Reset alarm counters
        alarmCount = 0;
        alarmEnabled = TRUE;

        // Send DISC frame
        unsigned char discFrame[5];
        discFrame[0] = FLAG;
        discFrame[1] = A_EMISSOR;
        discFrame[2] = C_DISC;
        discFrame[3] = (A_EMISSOR ^ C_DISC);
        discFrame[4] = FLAG;

        int attempts = 0;
        int maxAttempts = connectionParameters.nRetransmissions;
        
        while (attempts < maxAttempts) {
            // Send DISC
            int bytes = writeBytesSerialPort(discFrame, 5);
            printf("DISC frame sent (%d bytes)\n", bytes);

            // Set alarm
            alarm(connectionParameters.timeout);
            alarmEnabled = TRUE;

            // Wait for DISC response from receiver
            int state = UA_START_STATE;
            unsigned char byte;
            int discReceived = FALSE;

            while (state != UA_STOP_STATE && alarmEnabled) {
                if (readByteSerialPort(&byte) <= 0) {
                    continue;
                }

                switch (state) {
                    case UA_START_STATE:
                        if (byte == FLAG_RCV) {
                            state = UA_FLAG_RCV_STATE;
                        }
                        break;
                    case UA_FLAG_RCV_STATE:
                        if (byte == FLAG_RCV) {
                            state = UA_FLAG_RCV_STATE;
                        } else if (byte == A_RCV) {
                            state = UA_A_RCV_STATE;
                        } else {
                            state = UA_START_STATE;
                        }
                        break;
                    case UA_A_RCV_STATE:
                        if (byte == FLAG_RCV) {
                            state = UA_FLAG_RCV_STATE;
                        } else if (byte == C_DISC) {
                            state = UA_C_RCV_STATE;
                        } else {
                            state = UA_START_STATE;
                        }
                        break;
                    case UA_C_RCV_STATE:
                        if (byte == FLAG_RCV) {
                            state = UA_FLAG_RCV_STATE;
                        } else if (byte == (A_RCV ^ C_DISC)) {
                            state = UA_BCC_OK_STATE;
                        } else {
                            state = UA_START_STATE;
                        }
                        break;
                    case UA_BCC_OK_STATE:
                        if (byte == FLAG_RCV) {
                            state = UA_STOP_STATE;
                            discReceived = TRUE;
                        } else {
                            state = UA_START_STATE;
                        }
                        break;
                    case UA_STOP_STATE:
                        break;
                }
            }

            alarm(0); // Disable alarm

            if (discReceived) {
                printf("DISC response received\n");
                
                // Send UA frame
                unsigned char uaFrame[5];
                uaFrame[0] = FLAG;
                uaFrame[1] = A_EMISSOR;
                uaFrame[2] = C_UA;
                uaFrame[3] = (A_EMISSOR ^ C_UA);
                uaFrame[4] = FLAG;

                writeBytesSerialPort(uaFrame, 5);
                printf("UA frame sent\n");
                
                sleep(1); // Wait for UA to be sent
                break;
            }

            attempts++;
            printf("Timeout waiting for DISC response, attempt %d/%d\n", attempts, maxAttempts);
        }

        if (attempts >= maxAttempts) {
            fprintf(stderr, "Failed to disconnect after %d attempts\n", maxAttempts);
            closeSerialPort();
            return -1;
        }

    } else if (connectionParameters.role == LlRx) { // Receiver
        // Wait for DISC from transmitter
        int state = SET_START_STATE;
        unsigned char byte;

        while (state != SET_STOP_STATE) {
            if (readByteSerialPort(&byte) <= 0) {
                continue;
            }

            switch (state) {
                case SET_START_STATE:
                    if (byte == FLAG_RCV) {
                        state = SET_FLAG_RCV_STATE;
                    }
                    break;
                case SET_FLAG_RCV_STATE:
                    if (byte == A_TRANSMITTER) {
                        state = SET_A_RCV_STATE;
                    } else if (byte != FLAG_RCV) {
                        state = SET_START_STATE;
                    }
                    break;
                case SET_A_RCV_STATE:
                    if (byte == C_DISC) {
                        state = SET_C_RCV_STATE;
                    } else if (byte == FLAG_RCV) {
                        state = SET_FLAG_RCV_STATE;
                    } else {
                        state = SET_START_STATE;
                    }
                    break;
                case SET_C_RCV_STATE:
                    if (byte == (A_TRANSMITTER ^ C_DISC)) {
                        state = SET_BCC_OK_STATE;
                    } else if (byte == FLAG_RCV) {
                        state = SET_FLAG_RCV_STATE;
                    } else {
                        state = SET_START_STATE;
                    }
                    break;
                case SET_BCC_OK_STATE:
                    if (byte == FLAG_RCV) {
                        state = SET_STOP_STATE;
                    } else {
                        state = SET_START_STATE;
                    }
                    break;
                case SET_STOP_STATE:
                    break;
            }
        }

        printf("DISC frame received\n");

        // Send DISC response
        unsigned char discFrame[5];
        discFrame[0] = FLAG;
        discFrame[1] = A_RCV;
        discFrame[2] = C_DISC;
        discFrame[3] = (A_RCV ^ C_DISC);
        discFrame[4] = FLAG;

        writeBytesSerialPort(discFrame, 5);
        printf("DISC response sent\n");

        // Wait for UA
        state = SET_START_STATE;
        while (state != SET_STOP_STATE) {
            if (readByteSerialPort(&byte) <= 0) {
                continue;
            }

            switch (state) {
                case SET_START_STATE:
                    if (byte == FLAG_RCV) {
                        state = SET_FLAG_RCV_STATE;
                    }
                    break;
                case SET_FLAG_RCV_STATE:
                    if (byte == A_TRANSMITTER) {
                        state = SET_A_RCV_STATE;
                    } else if (byte != FLAG_RCV) {
                        state = SET_START_STATE;
                    }
                    break;
                case SET_A_RCV_STATE:
                    if (byte == C_UA) {
                        state = SET_C_RCV_STATE;
                    } else if (byte == FLAG_RCV) {
                        state = SET_FLAG_RCV_STATE;
                    } else {
                        state = SET_START_STATE;
                    }
                    break;
                case SET_C_RCV_STATE:
                    if (byte == (A_TRANSMITTER ^ C_UA)) {
                        state = SET_BCC_OK_STATE;
                    } else if (byte == FLAG_RCV) {
                        state = SET_FLAG_RCV_STATE;
                    } else {
                        state = SET_START_STATE;
                    }
                    break;
                case SET_BCC_OK_STATE:
                    if (byte == FLAG_RCV) {
                        state = SET_STOP_STATE;
                    } else {
                        state = SET_START_STATE;
                    }
                    break;
                case SET_STOP_STATE:
                    break;
            }
        }

        printf("UA frame received\n");
    } else {
        fprintf(stderr, "Invalid role specified\n");
        return -1;
    }

    // Close serial port
    if (closeSerialPort() < 0) {
        perror("closeSerialPort");
        return -1;
    }

    printf("Connection closed successfully\n");
    return 0;
}
