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
#include <time.h>

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
    printf("ALARM: Timeout triggered!\n");
    fflush(stdout); 
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
    
    while (!conn_state.alarm_triggered) {
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
    if (bufSize <= 0 || bufSize > MAX_PAYLOAD_SIZE) {
        return -1;
    }
    
    unsigned char frame[MAX_PAYLOAD_SIZE * 2 + 10];
    int frame_size = build_information_frame(buf, bufSize, frame);
    
    if (frame_size < 0) {
        return -1;
    }
    
    configure_alarm_handler();
    
    for (conn_state.retry_count = 0; conn_state.retry_count < conn_state.max_retries; 
         conn_state.retry_count++) {
        
        // Send frame
        int bytes_written = write(conn_state.fd, frame, frame_size);
        if (bytes_written != frame_size) {
            printf("Write error: sent %d/%d bytes\n", bytes_written, frame_size);
            continue;
        }
        
        // Wait for response
        conn_state.alarm_triggered = 0;
        alarm(conn_state.timeout_duration);
        
        unsigned char response[5];
        int response_idx = 0;
        time_t start_time = time(NULL);
        
        while (!conn_state.alarm_triggered) {
            unsigned char byte;
            if (read(conn_state.fd, &byte, 1) == 1) {
                response[response_idx++] = byte;
                
                // Check if we have a complete supervision frame
                if (response_idx == 5 && response[4] == FRAME_FLAG) {
                    alarm(0);
                    
                    unsigned char addr = response[1];
                    unsigned char ctrl = response[2];
                    unsigned char bcc = response[3];
                    
                    // Verify BCC
                    if (bcc != (addr ^ ctrl)) {
                        break; // Invalid frame, will retry
                    }
                    
                    // Check for RR with correct sequence
                    if (ctrl == CTRL_RR(!conn_state.current_sequence)) {
                        conn_state.current_sequence = !conn_state.current_sequence;
                        return bufSize;
                    }
                    
                    // Check for REJ - resend immediately
                    if (ctrl == CTRL_REJ(conn_state.current_sequence)) {
                        printf("Received REJ, retrying...\n");
                        break;
                    }
                }
                
                // Reset if we get a FLAG in the middle
                if (byte == FRAME_FLAG && response_idx < 5) {
                    response[0] = byte;
                    response_idx = 1;
                }
            }
            
            // Timeout check
            if (difftime(time(NULL), start_time) > conn_state.timeout_duration) {
                break;
            }
        }
        
        alarm(0);
        printf("Timeout or error on attempt %d/%d\n", 
               conn_state.retry_count + 1, conn_state.max_retries);
    }
    
    printf("Failed to send frame after %d attempts\n", conn_state.max_retries);
    return -1;
}
int llread(unsigned char *packet) {
    unsigned char frame[MAX_PAYLOAD_SIZE * 2 + 10];
    int frame_idx = 0;
    unsigned char byte;
    int in_escape = 0;
    
    enum { WAIT_START_FLAG, READ_ADDR, READ_CTRL, READ_BCC1, READ_DATA, CHECK_END_FLAG } state = WAIT_START_FLAG;
    
    unsigned char addr, ctrl, bcc1;
    
    while (1) {
        if (read(conn_state.fd, &byte, 1) != 1) {
            continue;
        }
        
        switch (state) {
            case WAIT_START_FLAG:
                if (byte == FRAME_FLAG) {
                    frame_idx = 0;
                    in_escape = 0;
                    state = READ_ADDR;
                }
                break;
                
            case READ_ADDR:
                if (byte == FRAME_FLAG) {
                    state = READ_ADDR; // Still waiting for address
                } else {
                    addr = byte;
                    state = READ_CTRL;
                }
                break;
                
            case READ_CTRL:
                if (byte == FRAME_FLAG) {
                    state = WAIT_START_FLAG;
                } else {
                    ctrl = byte;
                    state = READ_BCC1;
                }
                break;
                
            case READ_BCC1:
                if (byte == FRAME_FLAG) {
                    state = WAIT_START_FLAG;
                } else {
                    bcc1 = byte;
                    // Verify BCC1
                    if (bcc1 != (addr ^ ctrl)) {
                        printf("BCC1 error\n");
                        state = WAIT_START_FLAG;
                    } else {
                        // Check if this is a supervision frame (DISC, RR, REJ)
                        if (ctrl == CTRL_DISC) {
                            return 0; // Signal disconnection
                        } else if ((ctrl & 0x0F) == 0x05 || (ctrl & 0x0F) == 0x01) {
                            // RR or REJ - ignore in llread
                            state = WAIT_START_FLAG;
                        } else {
                            state = READ_DATA;
                        }
                    }
                }
                break;
                
            case READ_DATA:
                if (byte == FRAME_FLAG) {
                    // End of frame - destuff and verify BCC2
                    if (frame_idx < 1) {
                        state = WAIT_START_FLAG;
                        break;
                    }
                    
                    // Last byte should be BCC2
                    unsigned char received_bcc2 = frame[frame_idx - 1];
                    frame_idx--;
                    
                    // Calculate BCC2 on actual data
                    unsigned char calculated_bcc2 = 0;
                    for (int i = 0; i < frame_idx; i++) {
                        calculated_bcc2 ^= frame[i];
                    }
                    
                    if (calculated_bcc2 != received_bcc2) {
                        printf("BCC2 error: expected 0x%02X, got 0x%02X\n", 
                               calculated_bcc2, received_bcc2);
                        // Send REJ
                        transmit_supervision_frame(conn_state.fd, ADDR_RECEIVER, 
                                                  CTRL_REJ(conn_state.current_sequence));
                        state = WAIT_START_FLAG;
                        break;
                    }
                    
                    // Check sequence number
                    unsigned char expected_ctrl = CTRL_INFO(conn_state.current_sequence);
                    if (ctrl != expected_ctrl) {
                        printf("Wrong sequence: expected %d, got %d\n", 
                               conn_state.current_sequence, (ctrl >> 6) & 1);
                        // Send RR for next expected
                        transmit_supervision_frame(conn_state.fd, ADDR_RECEIVER,
                                                  CTRL_RR(!conn_state.current_sequence));
                        state = WAIT_START_FLAG;
                        break;
                    }
                    
                    // Success - copy data and send RR
                    memcpy(packet, frame, frame_idx);
                    transmit_supervision_frame(conn_state.fd, ADDR_RECEIVER,
                                              CTRL_RR(!conn_state.current_sequence));
                    conn_state.current_sequence = !conn_state.current_sequence;
                    
                    return frame_idx;
                } else {
                    // Handle byte stuffing
                    if (in_escape) {
                        frame[frame_idx++] = byte ^ 0x20;
                        in_escape = 0;
                    } else if (byte == ESCAPE_BYTE) {
                        in_escape = 1;
                    } else {
                        frame[frame_idx++] = byte;
                    }
                    
                    // Prevent buffer overflow
                    if (frame_idx >= MAX_PAYLOAD_SIZE * 2) {
                        printf("Frame too large\n");
                        state = WAIT_START_FLAG;
                    }
                }
                break;
                
            case CHECK_END_FLAG:
                // Not used in this implementation
                break;
        }
    }
    
    return -1;
}
int llclose(int showStatistics) {
    int result = -1;
    
    if (conn_state.role == LlTx) {
        // Transmitter initiates disconnection
        configure_alarm_handler();
        
        for (conn_state.retry_count = 0; 
             conn_state.retry_count < conn_state.max_retries; 
             conn_state.retry_count++) {
            
            printf("Sending DISC (attempt %d/%d)...\n", 
                   conn_state.retry_count + 1, conn_state.max_retries);
            
            if (transmit_supervision_frame(conn_state.fd, ADDR_SENDER, CTRL_DISC) < 0) {
                continue;
            }
            
            // Wait for DISC response
            conn_state.alarm_triggered = 0;
            alarm(conn_state.timeout_duration);
            
            unsigned char response[5];
            int idx = 0;
            int got_disc = 0;
            
            while (!conn_state.alarm_triggered && idx < 5) {
                unsigned char byte;
                if (read(conn_state.fd, &byte, 1) == 1) {
                    response[idx++] = byte;
                    
                    if (idx == 5 && response[4] == FRAME_FLAG) {
                        unsigned char ctrl = response[2];
                        unsigned char bcc = response[3];
                        
                        if (bcc == (response[1] ^ ctrl) && ctrl == CTRL_DISC) {
                            got_disc = 1;
                            break;
                        }
                    }
                    
                    if (byte == FRAME_FLAG && idx < 5) {
                        response[0] = byte;
                        idx = 1;
                    }
                }
            }
            
            alarm(0);
            
            if (got_disc) {
                printf("Received DISC, sending UA...\n");
                transmit_supervision_frame(conn_state.fd, ADDR_SENDER, CTRL_UA);
                sleep(1); // Give receiver time to process
                result = 0;
                break;
            }
        }
        
        if (result != 0) {
            printf("Failed to disconnect properly\n");
        }
        
    } else {
        // Receiver waits for DISC
        printf("Waiting for DISC...\n");
        
        unsigned char response[5];
        int idx = 0;
        
        while (idx < 5) {
            unsigned char byte;
            if (read(conn_state.fd, &byte, 1) == 1) {
                response[idx++] = byte;
                
                if (idx == 5 && response[4] == FRAME_FLAG) {
                    unsigned char ctrl = response[2];
                    unsigned char bcc = response[3];
                    
                    if (bcc == (response[1] ^ ctrl) && ctrl == CTRL_DISC) {
                        printf("Received DISC, sending DISC...\n");
                        transmit_supervision_frame(conn_state.fd, ADDR_RECEIVER, CTRL_DISC);
                        
                        // Wait for UA
                        idx = 0;
                        while (idx < 5) {
                            if (read(conn_state.fd, &byte, 1) == 1) {
                                response[idx++] = byte;
                                
                                if (idx == 5 && response[4] == FRAME_FLAG) {
                                    ctrl = response[2];
                                    if (ctrl == CTRL_UA) {
                                        printf("Received UA\n");
                                        result = 0;
                                    }
                                    break;
                                }
                                
                                if (byte == FRAME_FLAG && idx < 5) {
                                    response[0] = byte;
                                    idx = 1;
                                }
                            }
                        }
                        break;
                    }
                }
                
                if (byte == FRAME_FLAG && idx < 5) {
                    response[0] = byte;
                    idx = 1;
                }
            }
        }
    }
    
    if (showStatistics) {
        printf("\n=== Connection Statistics ===\n");
        printf("Role: %s\n", conn_state.role == LlTx ? "Transmitter" : "Receiver");
        printf("Total retries: %d\n", conn_state.retry_count);
        printf("Status: %s\n", result == 0 ? "Success" : "Failed");
    }
    
    closeSerialPort(conn_state.fd);
    return result;
}
