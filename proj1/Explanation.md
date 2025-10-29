## Explanation of application_layer.c

**Define the general variables, to distinguish between START, END and SIZE and NAME**

```c
// Control field values
#define CTRL_DATA 0x02
#define CTRL_START 0x01
#define CTRL_END 0x03

// TLV (Type-Length-Value) types
#define TLV_FILE_SIZE 0x00 // type file fize
#define TLV_FILE_NAME 0x01 // type file name
```

We will only use the `applicationLayer(...)` provided!

1. First we define the link layer parameters

```c
  // Link layer paramteres
  LinkLayer connectionParameters;
  connectionParameters.baudRate = baudRate;
  strcpy(connectionParameters.serialPort, serialPort); //more efficient that using .serialPort = serialPort!
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
```

If the role is not tx nor rx, we return!

- ## **Role == "tx"!**

Open a file with the `filename` as **read binary!**

```c
    FILE *file = fopen(filename, "rb"); // read binary mode

    if (file == NULL)
    {
      perror("Error opening file");
      return;
    }
```

Then we use fseek and ftell to dynamically know the size of the file!

```c
    /*
     * Dynamically know the size of any file given, no need for a special fileSize parameter!
     */
    fseek(file, 0, SEEK_END);    // moves the file pointer to the end of the file. By doing this, the program can calculate the total size of the file using ftell
    long fileSize = ftell(file); // get the fileSize, gets the location of the file pointer
    fseek(file, 0, SEEK_SET);    // reset the file pointer to the beginning
```

**Now we create the control packet!**

```c
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
```

We create a buffer named cP with initial size 50, to store all the information:
In this case we strat with the control packet START!

- cP[2]: Length field (2), specifying that the file size is encoded using 2 bytes.

**cp[3] and 4 represent the value itself**

- cP[3] and cP[4]: Value field, which should contain the actual file size encoded in big-endian format (most significant byte first).

### **Create the data packet to send to the receiver**

1. We create a temporary buffer to hold the file data, then use a `int rBytes = 0`, the number of total bytes

```c
while ((rBytes = fread(rbuf, sizeof(unsigned char), 1000, file)) > 0)
    {
      /*
      The fread function reads up to 1000 bytes from the file intro the rbuf buffer. The number of bytes is stored in rBytes. The loop continues until the end of the file is reached
      */

     // Now we proceed to building the data packet
      unsigned char dp[1100] = {0}; // data packet
      dp[0] = CTRL_DATA;
      dp[1] = (rBytes >> 8) & 0xFF; // L2
      dp[2] = rBytes & 0xFF;        // L1

      memcpy(&dp[3], rbuf, rBytes); // copies from rbuf, rBytes to dp[3]

      llwrite(dp, rBytes + 3); // writes to receiver all the content inside the Data Packet, with size rBytes + 3, because of the first 3 elements of dP!
    }
```

Now it is simple to create the **END** data packet. We just need to update the first element of cP, to 3 (which means it is a **END** control packet)!

- ## **Role == "rx"!**

We need to open a file, but now to write to it!

```c
    FILE *file = fopen(filename, "w"); // open file but to write

    unsigned char cpacket[600] = {0}; // create the data packet
    llread(cpacket);                  // read the data packet from the transmitter
```

We also call llread and save the content into cpacket!

```c

    while (STOP)
    {

      unsigned char packet[2000] = {0}; //create the packet to store the data

      int packetBytes = llread(packet); //the number of bytes read

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

```

In the end we close the connection using `llclose()`!
