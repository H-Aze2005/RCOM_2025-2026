// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Control field values
#define CTRL_DATA 0x02
#define CTRL_START 0x01
#define CTRL_END 0x03

// TLV (Type-Length-Value) types
#define TLV_FILE_SIZE 0x00 //type file fize
#define TLV_FILE_NAME 0x01 //type file name

// Maximum packet data size
#define MAX_DATA_SIZE 1000

/**
 * Build a control packet (START or END)
 * Returns the packet size
 */
 
int buildControlPacket(unsigned char *packet, unsigned char controlField, 
                       const char *filename, long fileSize) {
    int index = 0;
    
    // Control field
    packet[index++] = controlField;
    
    // TLV for file size
    packet[index++] = TLV_FILE_SIZE;
    unsigned char sizeLength = sizeof(long);
    packet[index++] = sizeLength;
    
    // File size in big-endian
    for (int i = sizeLength - 1; i >= 0; i--) {
        packet[index++] = (fileSize >> (i * 8)) & 0xFF;
    }
    
    // TLV for file name
    packet[index++] = TLV_FILE_NAME;
    unsigned char nameLength = strlen(filename);
    packet[index++] = nameLength;
    memcpy(packet + index, filename, nameLength);
    index += nameLength;
    
    return index;
}

/**
 * Parse a control packet
 * Returns 0 on success, -1 on error
 */
int parseControlPacket(const unsigned char *packet, int packetSize,
                       char *filename, long *fileSize) {
    int index = 0;
    
    // Skip control field
    index++;
    
    while (index < packetSize) {
        unsigned char type = packet[index++];
        unsigned char length = packet[index++];
        
        if (type == TLV_FILE_SIZE) {
            *fileSize = 0;
            for (int i = 0; i < length; i++) {
                *fileSize = (*fileSize << 8) | packet[index++];
            }
        } else if (type == TLV_FILE_NAME) {
            memcpy(filename, packet + index, length);
            filename[length] = '\0';
            index += length;
        } else {
            index += length; // Skip unknown TLV
        }
    }
    
    return 0;
}

/**
 * Build a data packet
 * Returns the packet size
 */
int buildDataPacket(unsigned char *packet, unsigned char sequenceNumber,
                    const unsigned char *data, int dataSize) {
    int index = 0;
    
    // Control field
    packet[index++] = CTRL_DATA;
    
    // Sequence number (modulo 256)
    packet[index++] = sequenceNumber;
    
    // Data size (2 bytes, big-endian)
    packet[index++] = (dataSize >> 8) & 0xFF;
    packet[index++] = dataSize & 0xFF;
    
    // Data
    memcpy(packet + index, data, dataSize);
    index += dataSize;
    
    return index;
}

/**
 * Transmitter application
 */
void transmitFile(LinkLayer connectionParameters, const char *filename) {
    // Open file
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }
    
    // Get file size
    struct stat st;
    if (stat(filename, &st) != 0) {
        perror("Error getting file size");
        fclose(file);
        return;
    }
    long fileSize = st.st_size;
    
    printf("Transmitting file: %s (%ld bytes)\n", filename, fileSize);
    
    // Open connection
    if (llopen(connectionParameters) < 0) {
        fprintf(stderr, "Error opening connection\n");
        fclose(file);
        return;
    }
    
    // Send START control packet
    unsigned char controlPacket[MAX_PAYLOAD_SIZE];
    int controlPacketSize = buildControlPacket(controlPacket, CTRL_START, 
                                                filename, fileSize);
    
    if (llwrite(controlPacket, controlPacketSize) < 0) {
        fprintf(stderr, "Error sending START packet\n");
        fclose(file);
        llclose(connectionParameters);
        return;
    }
    
    printf("START packet sent\n");
    
    // Send data packets
    unsigned char dataBuffer[MAX_DATA_SIZE];
    unsigned char dataPacket[MAX_PAYLOAD_SIZE];
    unsigned char sequenceNumber = 0;
    int bytesRead;
    long totalBytesSent = 0;
    
    while ((bytesRead = fread(dataBuffer, 1, MAX_DATA_SIZE, file)) > 0) {
        int dataPacketSize = buildDataPacket(dataPacket, sequenceNumber, 
                                             dataBuffer, bytesRead);
        
        if (llwrite(dataPacket, dataPacketSize) < 0) {
            fprintf(stderr, "Error sending data packet %d\n", sequenceNumber);
            fclose(file);
            llclose(connectionParameters);
            return;
        }
        
        totalBytesSent += bytesRead;
        printf("Sent data packet %d: %d bytes (Total: %ld/%ld)\n", 
               sequenceNumber, bytesRead, totalBytesSent, fileSize);
        
        sequenceNumber = (sequenceNumber + 1) % 256;
    }
    
    // Send END control packet
    controlPacketSize = buildControlPacket(controlPacket, CTRL_END, 
                                           filename, fileSize);
    
    if (llwrite(controlPacket, controlPacketSize) < 0) {
        fprintf(stderr, "Error sending END packet\n");
        fclose(file);
        llclose(connectionParameters);
        return;
    }
    
    printf("END packet sent\n");
    
    // Close connection
    if (llclose(connectionParameters) < 0) {
        fprintf(stderr, "Error closing connection\n");
    }
    
    fclose(file);
    printf("File transmission complete\n");
}

/**
 * Receiver application
 */
void receiveFile(LinkLayer connectionParameters, const char *filename) {
    // Open connection
    if (llopen(connectionParameters) < 0) {
        fprintf(stderr, "Error opening connection\n");
        return;
    }
    
    // Receive START control packet
    unsigned char packet[MAX_PAYLOAD_SIZE];
    int packetSize;
    
    packetSize = llread(packet);
    if (packetSize < 0 || packet[0] != CTRL_START) {
        fprintf(stderr, "Error receiving START packet\n");
        llclose(connectionParameters);
        return;
    }
    
    // Parse START packet
    char receivedFilename[256];
    long fileSize;
    if (parseControlPacket(packet, packetSize, receivedFilename, &fileSize) < 0) {
        fprintf(stderr, "Error parsing START packet\n");
        llclose(connectionParameters);
        return;
    }
    
    printf("Receiving file: %s (%ld bytes)\n", receivedFilename, fileSize);
    
    // Open output file
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Error creating output file");
        llclose(connectionParameters);
        return;
    }
    
    // Receive data packets
    long totalBytesReceived = 0;
    unsigned char expectedSequenceNumber = 0;
    
    while (1) {
        packetSize = llread(packet);
        
        if (packetSize < 0) {
            fprintf(stderr, "Error reading packet\n");
            break;
        }
        
        // Check if END packet
        if (packet[0] == CTRL_END) {
            printf("END packet received\n");
            break;
        }
        
        // Process data packet
        if (packet[0] == CTRL_DATA) {
            unsigned char sequenceNumber = packet[1];
            int dataSize = (packet[2] << 8) | packet[3];
            unsigned char *data = packet + 4;
            
            // Write data to file
            if (fwrite(data, 1, dataSize, file) != dataSize) {
                perror("Error writing to file");
                break;
            }
            
            totalBytesReceived += dataSize;
            printf("Received data packet %d: %d bytes (Total: %ld/%ld)\n",
                   sequenceNumber, dataSize, totalBytesReceived, fileSize);
            
            expectedSequenceNumber = (sequenceNumber + 1) % 256;
        }
    }
    
    // Close file and connection
    fclose(file);
    
    if (llclose(connectionParameters) < 0) {
        fprintf(stderr, "Error closing connection\n");
    }
    
    printf("File reception complete\n");
    
    // Verify file size
    if (totalBytesReceived == fileSize) {
        printf("File received successfully: %ld bytes\n", totalBytesReceived);
    } else {
        fprintf(stderr, "Warning: File size mismatch. Expected: %ld, Received: %ld\n",
                fileSize, totalBytesReceived);
    }
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    // Configure link layer parameters
    LinkLayer connectionParameters;
    strcpy(connectionParameters.serialPort, serialPort);
    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;
    
    // Determine role and execute
    if (strcmp(role, "tx") == 0) {
        connectionParameters.role = LlTx;
        transmitFile(connectionParameters, filename);
    } else if (strcmp(role, "rx") == 0) {
        connectionParameters.role = LlRx;
        receiveFile(connectionParameters, filename);
    } else {
        fprintf(stderr, "Invalid role: %s (must be 'tx' or 'rx')\n", role);
    }
}