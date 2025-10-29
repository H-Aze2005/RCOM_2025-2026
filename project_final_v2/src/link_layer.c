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

// Frame delimiters and control bytes
#define FRAME_FLAG 0x7E
#define ESCAPE_BYTE 0x7D
#define ADDR_SENDER 0x03
#define ADDR_RECEIVER 0x01
#define CTRL_SET 0x03
#define CTRL_DISC 0x0B
#define CTRL_UA 0x07
#define CTRL_RR(n) ((n) ? 0x85 : 0x05)
#define CTRL_REJ(n) ((n) ? 0x81 : 0x01)
#define CTRL_INFO(n) ((n) ? 0x40 : 0x00)

// Connection state
typedef struct {
    int fd;
    LinkLayerRole role;
    int timeout_duration;
    int max_retries;
    int current_sequence;
    volatile int alarm_triggered;
    int retry_count;
} ConnectionState;

static ConnectionState conn_state = {-1, LlTx, 3, 3, 0, 0, 0};

// Forward declarations
static int transmit_supervision_frame(int fd, unsigned char addr, unsigned char ctrl);
static int receive_supervision_frame(int fd, unsigned char expected_ctrl);
static int build_information_frame(const unsigned char *data, int length, unsigned char *frame);
static int extract_frame_data(const unsigned char *frame, int frame_len, unsigned char *data);
static unsigned char calculate_bcc(const unsigned char *data, int length);
static void alarm_handler(int signal);
static int setup_connection_transmitter(int fd);
static int setup_connection_receiver(int fd);

////////////////////////////////////////////////
// Alarm handling
////////////////////////////////////////////////
static void alarm_handler(int signal) {
    (void)signal;
    conn_state.alarm_triggered = 1;
}

static void configure_alarm_handler() {
    struct sigaction sa;
    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);
}

////////////////////////////////////////////////
// BCC calculation
////////////////////////////////////////////////
static unsigned char calculate_bcc(const unsigned char *data, int length) {
    unsigned char bcc = 0;
    for (int i = 0; i < length; i++) {
        bcc ^= data[i];
    }
    return bcc;
}

////////////////////////////////////////////////
// Frame transmission
////////////////////////////////////////////////
static int transmit_supervision_frame(int fd, unsigned char addr, unsigned char ctrl) {
    unsigned char frame[5];
    frame[0] = FRAME_FLAG;
    frame[1] = addr;
    frame[2] = ctrl;
    frame[3] = addr ^ ctrl;
    frame[4] = FRAME_FLAG;
    
    return write(fd, frame, 5) == 5 ? 0 : -1;
}

static int build_information_frame(const unsigned char *data, int length, unsigned char *frame) {
    int frame_idx = 0;
    
    // Start flag
    frame[frame_idx++] = FRAME_FLAG;
    frame[frame_idx++] = ADDR_SENDER;
    frame[frame_idx++] = CTRL_INFO(conn_state.current_sequence);
    frame[frame_idx++] = ADDR_SENDER ^ CTRL_INFO(conn_state.current_sequence);
    
    // Calculate BCC2
    unsigned char bcc2 = calculate_bcc(data, length);
    
    // Stuff data and BCC2
    for (int i = 0; i < length; i++) {
        if (data[i] == FRAME_FLAG || data[i] == ESCAPE_BYTE) {
            frame[frame_idx++] = ESCAPE_BYTE;
            frame[frame_idx++] = data[i] ^ 0x20;
        } else {
            frame[frame_idx++] = data[i];
        }
    }
    
    // Stuff BCC2
    if (bcc2 == FRAME_FLAG || bcc2 == ESCAPE_BYTE) {
        frame[frame_idx++] = ESCAPE_BYTE;
        frame[frame_idx++] = bcc2 ^ 0x20;
    } else {
        frame[frame_idx++] = bcc2;
    }
    
    // End flag
    frame[frame_idx++] = FRAME_FLAG;
    
    return frame_idx;
}

////////////////////////////////////////////////
// Frame reception
////////////////////////////////////////////////
static int receive_supervision_frame(int fd, unsigned char expected_ctrl) {
    unsigned char byte;
    enum { WAIT_FLAG, WAIT_ADDR, WAIT_CTRL, WAIT_BCC, WAIT_END } state = WAIT_FLAG;
    unsigned char addr, ctrl;
    
    while (1) {
        if (read(fd, &byte, 1) != 1) continue;
        
        switch (state) {
            case WAIT_FLAG:
                if (byte == FRAME_FLAG) state = WAIT_ADDR;
                break;
            case WAIT_ADDR:
                if (byte == FRAME_FLAG) break;
                addr = byte;
                state = WAIT_CTRL;
                break;
            case WAIT_CTRL:
                if (byte == FRAME_FLAG) { state = WAIT_ADDR; break; }
                ctrl = byte;
                state = WAIT_BCC;
                break;
            case WAIT_BCC:
                if (byte == FRAME_FLAG) { state = WAIT_ADDR; break; }
                if (byte == (addr ^ ctrl)) state = WAIT_END;
                else state = WAIT_FLAG;
                break;
            case WAIT_END:
                if (byte == FRAME_FLAG && ctrl == expected_ctrl) return 0;
                state = (byte == FRAME_FLAG) ? WAIT_ADDR : WAIT_FLAG;
                break;
        }
    }
    return -1;
}

static int extract_frame_data(const unsigned char *frame, int frame_len, unsigned char *data) {
    int data_idx = 0;
    int stuffed = 0;
    
    // Skip header (flag, addr, ctrl, bcc1)
    for (int i = 4; i < frame_len - 1; i++) {
        if (frame[i] == FRAME_FLAG) break;
        
        if (stuffed) {
            data[data_idx++] = frame[i] ^ 0x20;
            stuffed = 0;
        } else if (frame[i] == ESCAPE_BYTE) {
            stuffed = 1;
        } else {
            data[data_idx++] = frame[i];
        }
    }
    
    // Last byte is BCC2
    if (data_idx > 0) {
        unsigned char bcc2 = data[--data_idx];
        unsigned char calculated_bcc = calculate_bcc(data, data_idx);
        
        if (bcc2 != calculated_bcc) return -1;
    }
    
    return data_idx;
}

////////////////////////////////////////////////
// Connection setup
////////////////////////////////////////////////
static int setup_connection_transmitter(int fd) {
    configure_alarm_handler();
    
    while (conn_state.retry_count < conn_state.max_retries) {
        if (transmit_supervision_frame(fd, ADDR_SENDER, CTRL_SET) < 0) {
            return -1;
        }
        
        conn_state.alarm_triggered = 0;
        alarm(conn_state.timeout_duration);
        
        if (receive_supervision_frame(fd, CTRL_UA) == 0) {
            alarm(0);
            return 0;
        }
        
        if (conn_state.alarm_triggered) {
            conn_state.retry_count++;
            printf("Timeout - retry %d/%d\n", conn_state.retry_count, conn_state.max_retries);
        }
    }
    
    return -1;
}

static int setup_connection_receiver(int fd) {
    if (receive_supervision_frame(fd, CTRL_SET) < 0) {
        return -1;
    }
    
    return transmit_supervision_frame(fd, ADDR_RECEIVER, CTRL_UA);
}

////////////////////////////////////////////////
// Public API
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {
    conn_state.fd = openSerialPort(connectionParameters.serialPort,
                                    connectionParameters.baudRate);
    if (conn_state.fd < 0) return -1;
    
    conn_state.role = connectionParameters.role;
    conn_state.timeout_duration = connectionParameters.timeout;
    conn_state.max_retries = connectionParameters.nRetransmissions;
    conn_state.retry_count = 0;
    conn_state.current_sequence = 0;
    
    int result;
    if (conn_state.role == LlTx) {
        result = setup_connection_transmitter(conn_state.fd);
    } else {
        result = setup_connection_receiver(conn_state.fd);
    }
    
    return result == 0 ? conn_state.fd : -1;
}

int llwrite(const unsigned char *buf, int bufSize) {
    unsigned char frame[2 * bufSize + 10];
    int frame_size = build_information_frame(buf, bufSize, frame);
    
    configure_alarm_handler();
    conn_state.retry_count = 0;
    
    while (conn_state.retry_count < conn_state.max_retries) {
        if (write(conn_state.fd, frame, frame_size) != frame_size) {
            return -1;
        }
        
        conn_state.alarm_triggered = 0;
        alarm(conn_state.timeout_duration);
        
        unsigned char response_ctrl;
        if (receive_supervision_frame(conn_state.fd, CTRL_RR(!conn_state.current_sequence)) == 0) {
            alarm(0);
            conn_state.current_sequence = !conn_state.current_sequence;
            return bufSize;
        }
        
        if (conn_state.alarm_triggered) {
            conn_state.retry_count++;
        }
    }
    
    return -1;
}

int llread(unsigned char *packet) {
    unsigned char frame[MAX_PAYLOAD_SIZE * 2];
    int frame_idx = 0;
    unsigned char byte;
    enum { WAIT_FLAG, READ_HEADER, READ_DATA } state = WAIT_FLAG;
    
    while (1) {
        if (read(conn_state.fd, &byte, 1) != 1) continue;
        
        switch (state) {
            case WAIT_FLAG:
                if (byte == FRAME_FLAG) {
                    frame[frame_idx++] = byte;
                    state = READ_HEADER;
                }
                break;
            case READ_HEADER:
                frame[frame_idx++] = byte;
                if (frame_idx == 4) state = READ_DATA;
                break;
            case READ_DATA:
                frame[frame_idx++] = byte;
                if (byte == FRAME_FLAG && frame_idx > 5) {
                    int data_len = extract_frame_data(frame, frame_idx, packet);
                    
                    if (data_len >= 0) {
                        unsigned char expected_ctrl = CTRL_INFO(conn_state.current_sequence);
                        if (frame[2] == expected_ctrl) {
                            transmit_supervision_frame(conn_state.fd, ADDR_RECEIVER, 
                                                      CTRL_RR(!conn_state.current_sequence));
                            conn_state.current_sequence = !conn_state.current_sequence;
                            return data_len;
                        }
                    } else {
                        transmit_supervision_frame(conn_state.fd, ADDR_RECEIVER,
                                                  CTRL_REJ(conn_state.current_sequence));
                    }
                    frame_idx = 0;
                    state = WAIT_FLAG;
                }
                break;
        }
    }
    
    return -1;
}

int llclose(int showStatistics) {
    int result = -1;
    
    if (conn_state.role == LlTx) {
        configure_alarm_handler();
        conn_state.retry_count = 0;
        
        while (conn_state.retry_count < conn_state.max_retries) {
            transmit_supervision_frame(conn_state.fd, ADDR_SENDER, CTRL_DISC);
            
            conn_state.alarm_triggered = 0;
            alarm(conn_state.timeout_duration);
            
            if (receive_supervision_frame(conn_state.fd, CTRL_DISC) == 0) {
                alarm(0);
                transmit_supervision_frame(conn_state.fd, ADDR_SENDER, CTRL_UA);
                result = 0;
                break;
            }
            
            if (conn_state.alarm_triggered) conn_state.retry_count++;
        }
    } else {
        if (receive_supervision_frame(conn_state.fd, CTRL_DISC) == 0) {
            transmit_supervision_frame(conn_state.fd, ADDR_RECEIVER, CTRL_DISC);
            receive_supervision_frame(conn_state.fd, CTRL_UA);
            result = 0;
        }
    }
    
    closeSerialPort(conn_state.fd);
    
    if (showStatistics) {
        printf("=== Connection Statistics ===\n");
        printf("Role: %s\n", conn_state.role == LlTx ? "Transmitter" : "Receiver");
        printf("Retries used: %d/%d\n", conn_state.retry_count, conn_state.max_retries);
    }
    
    return result;
}