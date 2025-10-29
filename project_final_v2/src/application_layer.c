#include "application_layer.h"
#include "link_layer.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Control packet types
#define PKT_TYPE_DATA 1
#define PKT_TYPE_START 2
#define PKT_TYPE_END 3

// TLV field types
#define TLV_FILESIZE 0
#define TLV_FILENAME 1

// Helper structure for file transfer
typedef struct {
    long file_size;
    char filename[256];
    int sequence_number;
} TransferContext;

static TransferContext transfer_ctx = {0};

////////////////////////////////////////////////
// Control packet builders
////////////////////////////////////////////////
static int build_control_packet(unsigned char type, const char *filename, 
                                long file_size, unsigned char *packet) {
    int idx = 0;
    packet[idx++] = type;
    
    // Add file size TLV
    packet[idx++] = TLV_FILESIZE;
    packet[idx++] = sizeof(long);
    memcpy(&packet[idx], &file_size, sizeof(long));
    idx += sizeof(long);
    
    // Add filename TLV
    int name_len = strlen(filename);
    packet[idx++] = TLV_FILENAME;
    packet[idx++] = name_len;
    memcpy(&packet[idx], filename, name_len);
    idx += name_len;
    
    return idx;
}

static int parse_control_packet(const unsigned char *packet, int length,
                                char *filename, long *file_size) {
    if (length < 1) return -1;
    
    unsigned char type = packet[0];
    if (type != PKT_TYPE_START && type != PKT_TYPE_END) return -1;
    
    int idx = 1;
    while (idx < length) {
        unsigned char tlv_type = packet[idx++];
        unsigned char tlv_len = packet[idx++];
        
        if (tlv_type == TLV_FILESIZE) {
            memcpy(file_size, &packet[idx], sizeof(long));
        } else if (tlv_type == TLV_FILENAME) {
            memcpy(filename, &packet[idx], tlv_len);
            filename[tlv_len] = '\0';
        }
        
        idx += tlv_len;
    }
    
    return 0;
}

////////////////////////////////////////////////
// Data packet builders
////////////////////////////////////////////////
static int build_data_packet(int seq_num, const unsigned char *data, 
                            int data_len, unsigned char *packet) {
    int idx = 0;
    packet[idx++] = PKT_TYPE_DATA;
    packet[idx++] = seq_num % 256;
    packet[idx++] = (data_len >> 8) & 0xFF;
    packet[idx++] = data_len & 0xFF;
    memcpy(&packet[idx], data, data_len);
    
    return idx + data_len;
}

static int parse_data_packet(const unsigned char *packet, int length,
                            int *seq_num, unsigned char *data) {
    if (length < 4 || packet[0] != PKT_TYPE_DATA) return -1;
    
    *seq_num = packet[1];
    int data_len = (packet[2] << 8) | packet[3];
    
    if (data_len + 4 > length) return -1;
    
    memcpy(data, &packet[4], data_len);
    return data_len;
}

////////////////////////////////////////////////
// File operations
////////////////////////////////////////////////
static int send_file_contents(int fd, FILE *file, long file_size) {
    unsigned char read_buffer[MAX_PAYLOAD_SIZE - 4];
    unsigned char packet_buffer[MAX_PAYLOAD_SIZE];
    long bytes_sent = 0;
    int sequence = 0;
    
    printf("Starting file transfer...\n");
    
    while (bytes_sent < file_size) {
        int to_read = (file_size - bytes_sent > sizeof(read_buffer)) ? 
                      sizeof(read_buffer) : (file_size - bytes_sent);
        
        int bytes_read = fread(read_buffer, 1, to_read, file);
        if (bytes_read <= 0) {
            perror("File read error");
            return -1;
        }
        
        int packet_len = build_data_packet(sequence++, read_buffer, 
                                          bytes_read, packet_buffer);
        
        if (llwrite(packet_buffer, packet_len) < 0) {
            printf("Transfer failed at sequence %d\n", sequence - 1);
            return -1;
        }
        
        bytes_sent += bytes_read;
        
        if (bytes_sent % 10240 == 0 || bytes_sent == file_size) {
            printf("\rProgress: %ld/%ld bytes (%.1f%%)", 
                   bytes_sent, file_size, 
                   (bytes_sent * 100.0) / file_size);
            fflush(stdout);
        }
    }
    
    printf("\n");
    return 0;
}

static int receive_file_contents(int fd, FILE *file, long expected_size) {
    unsigned char packet_buffer[MAX_PAYLOAD_SIZE];
    unsigned char data_buffer[MAX_PAYLOAD_SIZE];
    long bytes_received = 0;
    int expected_seq = 0;
    
    printf("Receiving file...\n");
    
    while (bytes_received < expected_size) {
        int packet_len = llread(packet_buffer);
        if (packet_len < 0) {
            printf("Read error\n");
            return -1;
        }
        
        if (packet_buffer[0] == PKT_TYPE_END) {
            break;
        }
        
        if (packet_buffer[0] != PKT_TYPE_DATA) {
            continue;
        }
        
        int seq_num;
        int data_len = parse_data_packet(packet_buffer, packet_len, 
                                        &seq_num, data_buffer);
        
        if (data_len < 0) {
            printf("Invalid data packet\n");
            continue;
        }
        
        if (fwrite(data_buffer, 1, data_len, file) != (size_t)data_len) {
            perror("File write error");
            return -1;
        }
        
        bytes_received += data_len;
        expected_seq = (seq_num + 1) % 256;
        
        if (bytes_received % 10240 == 0 || bytes_received >= expected_size) {
            printf("\rProgress: %ld/%ld bytes (%.1f%%)", 
                   bytes_received, expected_size,
                   (bytes_received * 100.0) / expected_size);
            fflush(stdout);
        }
    }
    
    printf("\n");
    return 0;
}

////////////////////////////////////////////////
// Public API
////////////////////////////////////////////////
void applicationLayer(const char *serialPort, const char *role, 
                     int baudRate, int nTries, int timeout, 
                     const char *filename) {
    LinkLayer link_config;
    strncpy(link_config.serialPort, serialPort, sizeof(link_config.serialPort) - 1);
    link_config.role = (strcmp(role, "tx") == 0) ? LlTx : LlRx;
    link_config.baudRate = baudRate;
    link_config.nRetransmissions = nTries;
    link_config.timeout = timeout;
    
    int fd = llopen(link_config);
    if (fd < 0) {
        printf("Connection failed\n");
        return;
    }
    
    printf("Connection established on %s\n", serialPort);
    
    if (link_config.role == LlTx) {
        // Transmitter mode
        FILE *file = fopen(filename, "rb");
        if (!file) {
            perror("Cannot open file");
            llclose(1);
            return;
        }
        
        struct stat file_stat;
        stat(filename, &file_stat);
        long file_size = file_stat.st_size;
        
        printf("Sending file: %s (%ld bytes)\n", filename, file_size);
        
        // Send start control packet
        unsigned char ctrl_packet[MAX_PAYLOAD_SIZE];
        int ctrl_len = build_control_packet(PKT_TYPE_START, filename, 
                                           file_size, ctrl_packet);
        llwrite(ctrl_packet, ctrl_len);
        
        // Send file data
        send_file_contents(fd, file, file_size);
        
        // Send end control packet
        ctrl_len = build_control_packet(PKT_TYPE_END, filename, 
                                       file_size, ctrl_packet);
        llwrite(ctrl_packet, ctrl_len);
        
        fclose(file);
        printf("File sent successfully\n");
        
    } else {
        // Receiver mode
        unsigned char packet[MAX_PAYLOAD_SIZE];
        
        // Receive start control packet
        int packet_len = llread(packet);
        char received_filename[256];
        long file_size;
        
        if (parse_control_packet(packet, packet_len, 
                                received_filename, &file_size) < 0) {
            printf("Invalid start packet\n");
            llclose(1);
            return;
        }
        
        printf("Receiving file: %s (%ld bytes)\n", 
               received_filename, file_size);
        
        FILE *file = fopen(filename, "wb");
        if (!file) {
            perror("Cannot create file");
            llclose(1);
            return;
        }
        
        // Receive file data
        receive_file_contents(fd, file, file_size);
        
        // Receive end control packet
        packet_len = llread(packet);
        
        fclose(file);
        printf("File received successfully\n");
    }
    
    llclose(1);
}