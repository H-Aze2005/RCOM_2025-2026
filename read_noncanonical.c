// Example of how to read from the serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BAUDRATE 38400
#define BUF_SIZE 256

// Protocol definitions
#define FLAG 0x7E
#define A_TRANSMITTER 0x03
#define A_RECEIVER 0x01
#define C_SET 0x03
#define C_UA 0x07
#define C_DISC 0x0B

// State machine states
typedef enum {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP_STATE
} StateMachine;

// Function prototypes for state machine
int processStateMachine(unsigned char byte, StateMachine *state, unsigned char *addressField, unsigned char *controlField);
int receiveSupervisionFrame(unsigned char expectedA, unsigned char expectedC);

int fd = -1;           // File descriptor for open serial port
struct termios oldtio; // Serial port settings to restore on closing
volatile int STOP = FALSE;

int openSerialPort(const char *serialPort, int baudRate);
int closeSerialPort();
int readByteSerialPort(unsigned char *byte);
int writeBytesSerialPort(const unsigned char *bytes, int nBytes);

// ---------------------------------------------------
// MAIN
// ---------------------------------------------------
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS0\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    //
    // NOTE: See the implementation of the serial port library in "serial_port/".
    const char *serialPort = argv[1];

    if (openSerialPort(serialPort, BAUDRATE) < 0)
    {
        perror("openSerialPort");
        exit(-1);
    }

    printf("Serial port %s opened\n", serialPort);

    // Example: Receive a SET frame using the state machine
    printf("Waiting for SET frame...\n");
    
    if (receiveSupervisionFrame(A_TRANSMITTER, C_SET) == 0) {
        printf("SET frame received successfully!\n");
        
        // Send UA (Unnumbered Acknowledgment) frame as response
        printf("Sending UA acknowledgment frame...\n");
        unsigned char uaFrame[5];
        uaFrame[0] = FLAG;
        uaFrame[1] = A_RECEIVER;  // We are the receiver responding
        uaFrame[2] = C_UA;        // UA control field
        uaFrame[3] = A_RECEIVER ^ C_UA;  // BCC calculation
        uaFrame[4] = FLAG;
        
        int bytesWritten = writeBytesSerialPort(uaFrame, 5);
        if (bytesWritten == 5) {
            printf("UA frame sent successfully (%d bytes)\n", bytesWritten);
        } else {
            printf("Error sending UA frame (sent %d bytes instead of 5)\n", bytesWritten);
        }
    } else {
        printf("Error receiving SET frame\n");
    }

    // Read from serial port until the 'z' char is received.

    // NOTE: This while() cycle is a simple example showing how to read from the serial port.
    // It must be changed in order to respect the specifications of the protocol indicated in the Lab guide.

    // TODO: Save the received bytes in a buffer array and print it at the end of the program.
    int nBytesBuf = 0;
    unsigned char buf[BUF_SIZE];

    while (STOP_STATE == FALSE)
    {
        // Read one byte from serial port.
        // NOTE: You must check how many bytes were actually read by reading the return value.
        // In this example, we assume that the byte is always read, which may not be true.
        unsigned char byte;
        int bytes = readByteSerialPort(&byte);
        nBytesBuf += bytes;

        printf("%c", byte);

       if (byte == '\n')
        {
            printf("Received end of line char. Stop reading from serial port.\n");
            STOP = TRUE;
        }
 
        buf[nBytesBuf] = byte;

    }

    printf("Total bytes received: %d\n", nBytesBuf);

    //writeBytesSerialPort(buf, nBytesBuf + 1);

    // Close serial port
    if (closeSerialPort() < 0)
    {
        perror("closeSerialPort");
        exit(-1);
    }

    printf("Serial port %s closed\n", serialPort);

    return 0;
}

// ---------------------------------------------------
// STATE MACHINE IMPLEMENTATION
// ---------------------------------------------------

/**
 * Process the state machine for frame reception
 * Returns 1 if frame is complete, 0 if still processing, -1 on error
 */
int processStateMachine(unsigned char byte, StateMachine *state, unsigned char *addressField, unsigned char *controlField)
{
    switch (*state) {
        case START:
            if (byte == FLAG) {
                *state = FLAG_RCV;
                printf("State: FLAG_RCV (received FLAG: 0x%02X)\n", byte);
            }
            // Stay in START state for any other byte
            break;
            
        case FLAG_RCV:
            if (byte == A_TRANSMITTER || byte == A_RECEIVER) {
                *state = A_RCV;
                *addressField = byte;
                printf("State: A_RCV (received Address: 0x%02X)\n", byte);
            } else if (byte == FLAG) {
                // Stay in FLAG_RCV state if another FLAG is received
                printf("State: FLAG_RCV (received another FLAG: 0x%02X)\n", byte);
            } else {
                // Go back to START for any other byte
                *state = START;
                printf("State: START (unexpected byte: 0x%02X)\n", byte);
            }
            break;
            
        case A_RCV:
            if (byte == C_SET || byte == C_UA || byte == C_DISC) {
                *state = C_RCV;
                *controlField = byte;
                printf("State: C_RCV (received Control: 0x%02X)\n", byte);
            } else if (byte == FLAG) {
                *state = FLAG_RCV;
                printf("State: FLAG_RCV (received FLAG: 0x%02X)\n", byte);
            } else {
                *state = START;
                printf("State: START (unexpected byte: 0x%02X)\n", byte);
            }
            break;
            
        case C_RCV:
            // Calculate expected BCC: A XOR C
            unsigned char expectedBCC = *addressField ^ *controlField;
            if (byte == expectedBCC) {
                *state = BCC_OK;
                printf("State: BCC_OK (received BCC: 0x%02X, expected: 0x%02X)\n", byte, expectedBCC);
            } else if (byte == FLAG) {
                *state = FLAG_RCV;
                printf("State: FLAG_RCV (received FLAG: 0x%02X)\n", byte);
            } else {
                *state = START;
                printf("State: START (wrong BCC: 0x%02X, expected: 0x%02X)\n", byte, expectedBCC);
            }
            break;
            
        case BCC_OK:
            if (byte == FLAG) {
                *state = STOP_STATE;
                printf("State: STOP_STATE (received final FLAG: 0x%02X)\n", byte);
                return 1; // Frame complete
            } else {
                *state = START;
                printf("State: START (expected final FLAG, got: 0x%02X)\n", byte);
            }
            break;
            
        case STOP_STATE:
            // Should not reach here
            *state = START;
            break;
    }
    
    return 0; // Still processing
}

/**
 * Receive a supervision frame (SET, UA, DISC) using the state machine
 * Returns 0 on success, -1 on error
 */
int receiveSupervisionFrame(unsigned char expectedA, unsigned char expectedC)
{
    StateMachine state = START;
    unsigned char addressField = 0;
    unsigned char controlField = 0;
    unsigned char byte;
    int timeout = 0;
    const int MAX_TIMEOUT = 1000; // Maximum attempts before timeout
    
    printf("Starting state machine to receive frame (A=0x%02X, C=0x%02X)\n", expectedA, expectedC);
    
    while (state != STOP_STATE && timeout < MAX_TIMEOUT) {
        int bytesRead = readByteSerialPort(&byte);
        
        if (bytesRead > 0) {
            printf("Read byte: 0x%02X\n", byte);
            
            int result = processStateMachine(byte, &state, &addressField, &controlField);
            
            if (result == 1) {
                // Frame received successfully, verify it matches expected values
                if (addressField == expectedA && controlField == expectedC) {
                    printf("Frame received successfully: A=0x%02X, C=0x%02X\n", addressField, controlField);
                    return 0;
                } else {
                    printf("Frame received but doesn't match expected values. Expected: A=0x%02X, C=0x%02X, Got: A=0x%02X, C=0x%02X\n", 
                           expectedA, expectedC, addressField, controlField);
                    // Reset state machine to continue looking for the correct frame
                    state = START;
                }
            }
            
            timeout = 0; // Reset timeout on successful read
        } else {
            timeout++;
            usleep(1000); // Wait 1ms before trying again
        }
    }
    
    if (timeout >= MAX_TIMEOUT) {
        printf("Timeout waiting for supervision frame\n");
        return -1;
    }
    
    return -1;
}

// ---------------------------------------------------
// SERIAL PORT LIBRARY IMPLEMENTATION
// ---------------------------------------------------

// Open and configure the serial port.
// Returns -1 on error.
int openSerialPort(const char *serialPort, int baudRate)
{
    // Open with O_NONBLOCK to avoid hanging when CLOCAL
    // is not yet set on the serial port (changed later)
    int oflags = O_RDWR | O_NOCTTY | O_NONBLOCK;
    fd = open(serialPort, oflags);
    if (fd < 0)
    {
        perror(serialPort);
        return -1;
    }

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        return -1;
    }

    // Convert baud rate to appropriate flag

    // Baudrate settings are defined in <asm/termbits.h>, which is included by <termios.h>
#define CASE_BAUDRATE(baudrate) \
    case baudrate:              \
        br = B##baudrate;       \
        break;

    tcflag_t br;
    switch (baudRate)
    {
        CASE_BAUDRATE(1200);
        CASE_BAUDRATE(1800);
        CASE_BAUDRATE(2400);
        CASE_BAUDRATE(4800);
        CASE_BAUDRATE(9600);
        CASE_BAUDRATE(19200);
        CASE_BAUDRATE(38400);
        CASE_BAUDRATE(57600);
        CASE_BAUDRATE(115200);
    default:
        fprintf(stderr, "Unsupported baud rate (must be one of 1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200)\n");
        return -1;
    }
#undef CASE_BAUDRATE

    // New port settings
    struct termios newtio;
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = br | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Block reading
    newtio.c_cc[VMIN] = 1;  // Byte by byte

    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        close(fd);
        return -1;
    }

    // Clear O_NONBLOCK flag to ensure blocking reads
    oflags ^= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, oflags) == -1)
    {
        perror("fcntl");
        close(fd);
        return -1;
    }

    return fd;
}

// Restore original port settings and close the serial port.
// Returns 0 on success and -1 on error.
int closeSerialPort()
{
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        return -1;
    }

    return close(fd);
}

// Wait up to 0.1 second (VTIME) for a byte received from the serial port.
// Must check whether a byte was actually received from the return value.
// Save the received byte in the "byte" pointer.
// Returns -1 on error, 0 if no byte was received, 1 if a byte was received.
int readByteSerialPort(unsigned char *byte)
{
    return read(fd, byte, 1);
}

// Write up to numBytes from the "bytes" array to the serial port.
// Must check how many were actually written in the return value.
// Returns -1 on error, otherwise the number of bytes written.
int writeBytesSerialPort(const unsigned char *bytes, int nBytes)
{
    return write(fd, bytes, nBytes);
}
