# Presentation Preparation - Serial Port Protocol Project

## Application Layer Questions

### 1. Guarantee Independence
The application layer maintains independence from the link layer through a clean interface defined in `application_layer.h`. The application layer uses the link layer functions `llopen()`, `llwrite()`, `llread()`, and `llclose()` without knowing the internal protocol details (frames, stuffing, error control, etc.).

### 2. File Characteristics Obtained on Tx
On the transmitter side, file characteristics are obtained using:
- `stat()` system call to get file size
- `fopen()` to open the file
- File name from command line arguments (passed to `applicationLayer()`)

### 3. TLV Fields Used on Tx
The transmitter typically uses:
- **T (Type)**: Field type identifier (e.g., 0 for file size, 1 for file name)
- **L (Length)**: Length of the value field
- **V (Value)**: The actual data (file size in bytes, file name string)

Common TLV fields:
- File size TLV
- File name TLV

### 4. Encoding the Length Field
The length field is typically encoded as:
- Multi-byte integer (e.g., 4 bytes for 32-bit length)
- Big-endian or little-endian byte order
- Example: For a file size of 12345 bytes, encode as 4 bytes: `0x00 0x00 0x30 0x39`

### 5. TLV Fields Usage on Rx
On the receiver side:
1. Read the **T** byte to identify field type
2. Read the **L** bytes to know how many value bytes to read
3. Read **V** bytes according to length
4. Process based on type (create file with name, allocate buffer for size, etc.)

### 6. Additional Functionalities
You can mention:
- **Statistics**: Track bytes sent/received, retransmissions, errors
- **Error logs**: Log protocol errors, timeouts, rejected frames
- **Progress indicators**: Show transmission progress
- **Validation**: Verify file integrity after transmission (compare sizes)

---

## Link Layer Questions

### 1. N(s) Field Creation and RR Verification on Tx

Looking at `link_layer.c`:

**N(s) Creation and Update**:
```c
static unsigned char sequenceNum = 0; // Initially 0
```
- N(s) alternates between 0 and 1
- In `llwrite()`, the control field is set: `C = 0x00` for I(0) or `C = 0x80` for I(1)
- After successful transmission (receiving RR), toggle: `sequenceNum ^= 1`

**RR0/RR1 Verification**:
- After sending I(0), expect RR1 (ready for frame 1)
- After sending I(1), expect RR0 (ready for frame 0)
- If wrong RR received, it's likely a duplicate acknowledgment → retransmit

### 2. Byte Stuffing (Tx) and Destuffing (Rx)

**Stuffing on Tx** (in `llwrite()`):
```c
// If FLAG (0x7E) appears in data:
newBuf[stuffedIndex++] = ESC;      // 0x7D
newBuf[stuffedIndex++] = 0x5E;     // 0x7E ^ 0x20

// If ESC (0x7D) appears in data:
newBuf[stuffedIndex++] = ESC;      // 0x7D
newBuf[stuffedIndex++] = 0x5D;     // 0x7D ^ 0x20
```

**Destuffing on Rx** (in `llread()`):
```c
if (byte == ESC) {
    readByteSerialPort(&byte);     // Read next byte
    byte ^= 0x20;                  // XOR with 0x20 to restore original
}
```

### 3. N(s) Verification on Rx for Duplication Detection

On the receiver side in `llread()`:

**Expected N(s) tracking**:
```c
static unsigned char expectedSeq = 0; // Start expecting I(0)
```

**Verification logic**:
1. Extract N(s) from control field: `receivedSeq = (controlField >> 7) & 0x01`
2. Compare with expected:
   - If `receivedSeq == expectedSeq`: **New frame** → process it, send RR, toggle expected
   - If `receivedSeq != expectedSeq`: **Duplicate** → discard data, resend last RR

**RR Generation**:
- After correctly receiving I(0), send RR1
- After correctly receiving I(1), send RR0

### 4. BCC1 Verification and Frame Discarding

In `llread()`, BCC1 verification:

```c
unsigned char bcc1 = addressField ^ controlField;

// In state machine:
case C_RECEIVED:
    if (byte == bcc1) {
        state = BCC_OK;  // BCC1 correct
    } else {
        state = START;   // BCC1 error → discard frame, restart
    }
```

**Error handling**:
- If BCC1 is incorrect, reset state machine to START
- Discard all received bytes
- Wait for next FLAG to start new frame
- **Do not send any response** (transmitter will timeout and retransmit)

### 5. BCC2 Verification and REJ Transmission

In `llread()`, BCC2 verification:

```c
unsigned char calculatedBcc2 = 0;
// XOR all data bytes
for (each data byte) {
    calculatedBcc2 ^= dataByte;
}

// After receiving all data + BCC2:
if (calculatedBcc2 == 0) {  // If XOR with BCC2 = 0, correct
    // Send RR0 or RR1
    return dataSize;
} else {
    // BCC2 error → send REJ0 or REJ1
    unsigned char rejFrame[5];
    rejFrame[0] = FLAG;
    rejFrame[1] = A_RCV;
    rejFrame[2] = (expectedSeq == 0) ? REJ0 : REJ1;  // Request retransmission
    rejFrame[3] = rejFrame[1] ^ rejFrame[2];
    rejFrame[4] = FLAG;
    writeBytesSerialPort(rejFrame, 5);
}
```

**REJ Logic**:
- If expecting I(0) and BCC2 fails → send REJ0 (reject frame 0, request retransmission)
- If expecting I(1) and BCC2 fails → send REJ1 (reject frame 1, request retransmission)
- Do **not** increment expected sequence number
- Transmitter will resend the same frame

---

## Key Points to Emphasize

1. **State machines** are used throughout for robust protocol handling
2. **Alarm/timeout mechanism** (using `sigaction()` and `alarm()`) handles lost frames
3. **Error detection** at multiple levels: BCC1 (header), BCC2 (data)
4. **Flow control** through alternating bit protocol (N(s) 0/1)
5. **Transparency** through byte stuffing ensures FLAG and ESC can appear in data

## Files to Review

- `link_layer.c` - Core protocol implementation
- `application_layer.c` - File transfer logic
- `serial_port.c` - Low-level serial port handling
- `main.c` - Entry point and parameter handling

Good luck with your presentation! 🎯