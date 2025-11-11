// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <string.h>
#include <stdio.h>

// Control field values
#define CTRL_DATA 0x02
#define CTRL_START 0x01
#define CTRL_END 0x03

// TLV (Type-Length-Value) types
#define TLV_FILE_SIZE 0x00 // type file fize
#define TLV_FILE_NAME 0x01 // type file name

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{

  // Link layer paramteres
  LinkLayer connectionParameters;
  connectionParameters.baudRate = baudRate;
  strcpy(connectionParameters.serialPort, serialPort);
  //------------------------------------------------------

  if (strcmp("tx", role) == 0)
  { // if it is transmitter role, change to if (role == "tx")
    connectionParameters.role = 0;
  }
  else
  { // if it is receiver
    connectionParameters.role = 1;
  }
  connectionParameters.timeout = 3;
  connectionParameters.nRetransmissions = nTries;
  int r = llopen(connectionParameters); // establish a link

  if (r != 0) // if is not receiver nor transmitter
  {
    return;
  }

  if (connectionParameters.role == 0) // if connection role is receiver
  {

    /*
     *The file specified is opened in read binary mode.
     *If file cannot be openeed then we return
     */
    FILE *file = fopen(filename, "rb"); // read binary mode

    if (file == NULL)
    {
      perror("Error opening file");
      return;
    }

    /*
     * Dynamically know the size of any file given, no need for a special fileSize parameter!
     */
    fseek(file, 0, SEEK_END);    // moves the file pointer to the end of the file. By doing this, the program can calculate the total size of the file using ftell
    long fileSize = ftell(file); // get the fileSize, gets the location of the file pointer
    fseek(file, 0, SEEK_SET);    // reset the file pointer to the beginning

    // Create the control packet

    unsigned char cP[50] = {0}; // change to controlPacket
    cP[0] = CTRL_START;
    cP[1] = TLV_FILE_SIZE; // file size
    cP[2] = 2;             // 2 bytes for the file size

    // Encode the file size into 2 bytes (big-endian format)
    // Encode the file size into 2 bytes (big-endian format)
    cP[3] = (fileSize >> 8) & 0xFF; // High byte of file size
    cP[4] = fileSize & 0xFF;        // Low byte of file size

    llwrite(cP, 5); // We invoke llwrite to write the controlpacket with size 5

    unsigned char rbuf[1000] = {0}; // buffer to temporarily hold chuncks of the file data
    int rBytes = 0;

    while ((rBytes = fread(rbuf, sizeof(unsigned char), 1000, file)) > 0)
    {
      /*
      The fread function reads up to 1000 bytes from the file intro the rbuf buffer. The number of bytes is stored in rBytes. The loop continues until the end of the file is reached
      */

      unsigned char dp[1100] = {0}; // data packet
      dp[0] = CTRL_DATA;
      dp[1] = (rBytes >> 8) & 0xFF; // L2
      dp[2] = rBytes & 0xFF;        // L1

      memcpy(&dp[3], rbuf, rBytes); // copies from rbuf, rBytes to dp[3]

      llwrite(dp, rBytes + 3); // writes to receiver all the content inside the Data Packet, with size rBytes + 3, because of the first 3 elements of dP!
    }

    // Now build the End Control Packet
    cP[0] = CTRL_END;
    // the rest od control packet will be the same

    llwrite(cP, 5); // write the control packet again to the receiver

    fclose(file); // close file

    llclose(); // close connection
  }

  if (connectionParameters.role == 1) // if we are now the receiver
  {

    FILE *file = fopen(filename, "w"); // open file but to write

    unsigned char cpacket[600] = {0}; // create the data packet
    llread(cpacket);                  // read the data packet from the transmitter
    int STOP = 1;
    while (STOP)
    {

      unsigned char packet[2000] = {0};

      int packetBytes = llread(packet);

      if (packetBytes > 0)
      {
        printf("Received packet: ");

        int pSize = packet[1] * 256 + packet[2];
        // K = 256 * l2 + l1

        printf("Packet Size = %d ", pSize);

        for (int i = 0; i < packetBytes; i++)
        {
          printf("0x%02X ", packet[i]);
        }
        if (packet[0] == CTRL_END)
        {
          STOP = 0;
          break;
        }

        fwrite(&packet[3], sizeof(unsigned char), pSize, file);
        // write the content of the dataPacket to the previously opened file!
      }
    }
    llclose(); // close connection
  }
  return;
}
