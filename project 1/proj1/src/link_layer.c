// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
LinkLayer info;
void alarmHandler(int signal);

volatile int STOP = FALSE;
int alarmEnabled = FALSE;
int alarmCount = 0;
unsigned char CurrentPacket = 0x00;

int llopen(LinkLayer connectionParameters) {

  info = connectionParameters;
  CurrentPacket = 0x00;
  alarmEnabled = FALSE;
  alarmCount = 0;
  const char *serialPort = connectionParameters.serialPort;
  State state = Start;

  if (openSerialPort(serialPort, connectionParameters.baudRate) < 0) {
    perror("openSerialPort");
    return -1;
  }

  printf("Serial port %s opened\n", serialPort);

  if (connectionParameters.role == 0) {

    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1) {
      perror("sigaction");
      return 1;
    }

    printf("Alarm configured\n");

    // Create string to send
    unsigned char buf[1024] = {0};

    buf[0] = FLAG;
    buf[1] = add1;
    buf[2] = SET;
    buf[3] = add1 ^ SET;
    buf[4] = FLAG;

    while (alarmCount < connectionParameters.nRetransmissions &&
           !alarmEnabled) {
      int bytes = writeBytesSerialPort(buf, 5);
      printf("Sending control word: ");
      int i = 0;

      while (i < 5) {
        printf("0x%02X ", buf[i]);
        i++;
      }
      printf("%d bytes written to serial port\n", bytes);
      sleep(1);
      if (alarmEnabled == FALSE) {
        alarm(connectionParameters.timeout);
        alarmEnabled = TRUE;
      }

      STOP = FALSE;
      unsigned char bufR[BUF_SIZE] = {0};
      while (STOP == FALSE) {
        if (!alarmEnabled) {
          break;
        }
        unsigned char byte;
        readByteSerialPort(&byte);

        switch (state) {
        case Start:
          printf("start\n");
          bufR[0] = '\0';
          if (byte == FLAG) {
            state = Flag_RCV;
            bufR[0] = FLAG;
          } else
            state = Start;

          break;

        case Flag_RCV:
          if (byte == FLAG)
            state = Flag_RCV;

          else if (byte == add1) {
            state = A_RCV;
            bufR[1] = add1;
          }

          else
            state = Start;

          break;

        case A_RCV:
          printf("a\n");
          if (byte == FLAG)
            state = Flag_RCV;

          else if (byte == UA) {
            state = C_RCV;
            bufR[2] = UA;
          }

          else
            state = Start;

          break;

        case C_RCV:
          printf("c\n");
          if (byte == FLAG)
            state = Flag_RCV;

          else if (byte == (UA ^ add1)) {
            bufR[3] = UA ^ add1;
            state = BCC_RCV;
          }

          else
            state = Start;

          break;

        case BCC_RCV:

          printf("bcc\n");
          if (byte == FLAG) {
            bufR[4] = FLAG;
            state = Stop;
            STOP = TRUE;
          }

          else
            state = Start;
          break;

        case Stop:
          printf("stop\n");
          STOP = TRUE;
          break;
        default:
          break;
        }
      }
      if (alarmEnabled) {
        printf("Received control word: ");
        alarmCount = 0;
        alarm(0);

        for (int i = 0; i < 5; i++) {
          printf("0x%02X ", bufR[i]);
        }
        break;
      }
    }

    return 0;
  } else {
    int nBytesBuf = 0;

    State state = Start;

    while (STOP == FALSE) {

      unsigned char byte;
      int bytes = readByteSerialPort(&byte);
      nBytesBuf += bytes;

      switch (state) {
      case Start:
        printf("start\n");
        if (byte == FLAG)
          state = Flag_RCV;

        else
          state = Start;

        break;

      case Flag_RCV:
        printf("flag\n");
        if (byte == FLAG)
          state = Flag_RCV;

        else if (byte == add1)
          state = A_RCV;

        else
          state = Start;

        break;

      case A_RCV:
        printf("a\n");
        if (byte == FLAG)
          state = Flag_RCV;

        else if (byte == SET)
          state = C_RCV;

        else
          state = Start;

        break;

      case C_RCV:
        printf("c\n");
        if (byte == FLAG)
          state = Flag_RCV;

        else if (byte == 0)
          state = BCC_RCV;

        else
          state = Start;

        break;

      case BCC_RCV:

        printf("bcc\n");
        if (byte == FLAG) {
          state = Stop;
          STOP = TRUE;
        }

        else
          state = Start;
        break;

      case Stop:
        printf("stop\n");
        STOP = TRUE;
        break;
      default:
        break;
      }
    }

    printf("SENDING NOW");
    unsigned char buf[BUF_SIZE] = {0};

    buf[0] = FLAG;
    buf[1] = add1;
    buf[2] = UA;
    buf[3] = add1 ^ UA;
    buf[4] = FLAG;

    sleep(1);
    writeBytesSerialPort(buf, 5);
    int i = 0;
    while (i < 5) {
      printf(" 0x%02X ", buf[i]);
      i++;
    }
    printf("\n");

    printf("Total bytes received: %d\n", nBytesBuf);

    return 0;
  }
}

void alarmHandler(int signal) {
  alarmEnabled = FALSE;
  alarmCount++;
  printf("Alarm #%d received\n", alarmCount);
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {
  alarmEnabled = FALSE;
  alarmCount = 0;
  int bytes_stuffed = 0;
  int REJ = TRUE;
  State state = Start;

  unsigned char tbuf[bufSize * 2 + 6];
  memset(tbuf, 0, bufSize * 2 + 6);

  tbuf[0] = FLAG;
  tbuf[1] = add1;
  tbuf[2] = CurrentPacket;
  tbuf[3] = add1 ^ CurrentPacket;

  unsigned char bcc2 = 0x00;
  for (int i = 0; i < bufSize; i++) {
    bcc2 = bcc2 ^ buf[i];
    if (buf[i] == 0x7E) {
      tbuf[4 + i + bytes_stuffed] = 0x7D;
      tbuf[4 + i + 1 + bytes_stuffed] = 0x5E;
      bytes_stuffed++;
    } else if (buf[i] == 0x7D) {
      tbuf[4 + i + bytes_stuffed] = 0x7D;
      tbuf[4 + i + 1 + bytes_stuffed] = 0x5D;
      bytes_stuffed++;
    } else {
      tbuf[4 + i + bytes_stuffed] = buf[i];
    }
  }

  tbuf[4 + bufSize + bytes_stuffed] = bcc2;
  tbuf[5 + bufSize + bytes_stuffed] = FLAG;

  struct sigaction act = {0};
  act.sa_handler = &alarmHandler;
  if (sigaction(SIGALRM, &act, NULL) == -1) {
    perror("sigaction");
    return 1;
  }
  int bytes = 0;
  printf("Alarm configured\n");
  REJ = TRUE;
  while ((alarmCount < info.nRetransmissions && !alarmEnabled) || REJ) {
    bytes = writeBytesSerialPort(tbuf, bufSize + 6 + bytes_stuffed);
    printf("Sending word: ");
    int i = 0;
    state = Start;
    while (i < 6 + bufSize + bytes_stuffed) {
      printf("0x%02X ", tbuf[i]);
      i++;
    }
    printf("\n");

    sleep(0.1);
    if (alarmEnabled == FALSE) {
      alarm(info.timeout);
      alarmEnabled = TRUE;
    }

    STOP = FALSE;
    unsigned char bufR[BUF_SIZE] = {0};
    while (STOP == FALSE) {
      if (!alarmEnabled) {
        break;
      }
      unsigned char byte;
      readByteSerialPort(&byte);

      switch (state) {
      case Start:
        printf("start\n");
        bufR[0] = '\0';
        if (byte == FLAG) {
          state = Flag_RCV;
          bufR[0] = FLAG;
        } else
          state = Start;

        break;

      case Flag_RCV:
        if (byte == FLAG)
          state = Flag_RCV;

        else if (byte == add1) {
          state = A_RCV;
          bufR[1] = add1;
        }

        else
          state = Start;

        break;

      case A_RCV:
        printf("a\n");
        if (byte == FLAG)
          state = Flag_RCV;

        else if (byte == 0xAB) {
          state = C_RCV;
          CurrentPacket = 0x80;
          REJ = FALSE;
          bufR[2] = byte;
        } else if (byte == 0xAA) {
          state = C_RCV;
          CurrentPacket = 0x00;
          REJ = FALSE;
          bufR[2] = byte;
        } else if (byte == 0x55) {
          REJ = TRUE;
          state = C_RCV;
          CurrentPacket = 0x80;
          bufR[2] = byte;
        } else if (byte == 0x54) {
          REJ = TRUE;
          state = C_RCV;
          CurrentPacket = 0x00;
          bufR[2] = byte;
        }

        else
          state = Start;

        break;

      case C_RCV:
        printf("c\n");
        if (byte == FLAG)
          state = Flag_RCV;

        else if (byte == (bufR[1] ^ bufR[2])) {
          bufR[3] = byte;
          state = BCC_RCV;
        } else
          state = Start;

        break;

      case BCC_RCV:

        printf("bcc\n");
        if (byte == FLAG) {
          bufR[4] = FLAG;
          writeBytesSerialPort(buf, 5);
          state = Stop;
          STOP = TRUE;
        }

        else
          state = Start;
        break;

      case Stop:
        printf("stop\n");
        STOP = TRUE;
        break;
      default:
        break;
      }
    }
    if (alarmEnabled) {
      printf("Received control word: ");
      alarmCount = 0;
      alarm(0);

      for (int i = 0; i < 5; i++) {
        printf("0x%02X ", bufR[i]);
      }
      if (bufR[2] == 0x54 || bufR[2] == 0x55) {
        printf("RESENDING: ");
        printf("%d \n", alarmEnabled);
        REJ = TRUE;
      } else {
        break;
      }
    }
  }

  return bytes;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {

  int nBytesBuf = 0;

  State state = Start;
  unsigned char ctrl = 0;
  int packetBytes = 0;
  unsigned char bcc1 = 0;
  int V_count = 0;
  int dataSize = 0;
  int dup = 0;
  unsigned char bcc2 = 0x00;
  unsigned char result[2006];
  STOP = FALSE;

  while (STOP == FALSE) {

    unsigned char byte;
    int bytes = readByteSerialPort(&byte);
    nBytesBuf += bytes;

    switch (state) {
    case Start:
      printf("start\n");
      if (byte == FLAG)
        state = Flag_RCV;

      else
        state = Start;

      break;

    case Flag_RCV:
      printf("flag\n");
      if (byte == FLAG)
        state = Flag_RCV;

      else if (byte == add1)
        state = A_RCV;

      else
        state = Start;

      break;

    case A_RCV:
      printf("a\n");
      if (byte == FLAG)
        state = Flag_RCV;

      else if (byte == CurrentPacket) {
        CurrentPacket = CurrentPacket ^ 0x80;
        bcc1 = (byte ^ add1);
        state = BCC_DATA;
      }

      else if (byte == (CurrentPacket ^ 0x80)) {
        state = BCC_DATA;
        bcc1 = (byte ^ add1);
        dup = 1;
      }

      else {
        ctrl = byte;
        state = C_RCV;
      }
      break;

    case C_RCV:
      printf("c\n");
      if (byte == FLAG)
        state = Flag_RCV;

      else if (byte == (add1 ^ ctrl))
        state = BCC_RCV;

      else
        state = Start;

      break;

    case BCC_DATA:
      printf("bccD\n");
      if (byte == FLAG)
        state = Flag_RCV;

      else if (byte == bcc1) {
        state = DATA_RCV;
      } else {
        state = Start;
      }
      break;

    case BCC_RCV:

      printf("bcc\n");

      if (ctrl == SET) {

        if (byte == FLAG) {

          state = Stop;
          STOP = TRUE;

          unsigned char buf[5] = {0};

          buf[0] = FLAG;
          buf[1] = add1;
          buf[2] = UA;
          buf[3] = add1 ^ UA;
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
      if (byte == FLAG) {
        state = STOP;
        STOP = TRUE;
      }

      else {
        state = Start;
      }
      break;

    case DATA_RCV:

      if (byte == FLAG) {
        state = Stop;
        STOP = TRUE;
      } 
      else {
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

  unsigned char test = 0x00;
  bcc2 = result[dataSize - 1];
  printf(" 0x%02X\n ", bcc2);
  int i = 0;
  int j = 0;
  while (i < dataSize - 1) {
    if (result[i] == 0x7D) {
        if (result[i+1] == 0x5D) {
            packet[j++] = 0x7D;
            i += 2;
        } else if (result[i+1] == 0x5E) {
            packet[j++] = 0x7E;
            i += 2;
        } else {
            // Unexpected escape sequence
            packet[j++] = result[i++];
        }
    } else {
        packet[j++] = result[i++];
    }
}
packetBytes = j;
  
  for (int i = 0; i < packetBytes; i++) {
    test = test ^ packet[i];
    printf(" 0x%02X ", packet[i]);
  }
  if (bcc2 == test) {
    unsigned char buf[5] = {0};
    buf[0] = FLAG;
    buf[1] = add1;
    buf[2] = 0xAA + (CurrentPacket == 0x80);
    buf[3] = add1 ^ buf[2];
    buf[4] = FLAG;
    int i = 0;
    printf("Sending: \n");
    while (i < 5) {
      printf(" 0x%02X ", buf[i]);
      i++;
    }

    writeBytesSerialPort(buf, 5);
  } else {
    CurrentPacket = (0x80 ^ CurrentPacket);
    unsigned char buf[5] = {0};
    buf[0] = FLAG;
    buf[1] = add1;
    buf[2] = 0x54 + (CurrentPacket == 0x80);
    buf[3] = add1 ^ buf[2];
    buf[4] = FLAG;

    writeBytesSerialPort(buf, 5);
    printf("REJECTED \n");
    return -1;
  }
  printf("\n");

  if (dup) {
    return -1;
  }

  return packetBytes;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose() {

  State state = Start;

  if (info.role == 0) {

    alarmCount = 0;
    alarmEnabled = FALSE;
    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1) {
      perror("sigaction");
      return 1;
    }

    printf("Alarm configured\n");

    // Create string to send
    unsigned char buf[1024] = {0};

    buf[0] = FLAG;
    buf[1] = add1;
    buf[2] = DISC;
    buf[3] = add1 ^ DISC;
    buf[4] = FLAG;

    while (alarmCount < info.nRetransmissions && !alarmEnabled) {
      int bytes = writeBytesSerialPort(buf, 5);
      printf("Sending control word: ");
      int i = 0;

      while (i < 5) {
        printf("0x%02X ", buf[i]);
        i++;
      }
      printf("%d bytes written to serial port\n", bytes);
      sleep(1);
      if (alarmEnabled == FALSE) {
        alarm(info.timeout);
        alarmEnabled = TRUE;
      }

      STOP = FALSE;
      unsigned char bufR[BUF_SIZE] = {0};
      while (STOP == FALSE) {
        if (!alarmEnabled) {
          break;
        }
        unsigned char byte;
        readByteSerialPort(&byte);

        switch (state) {
        case Start:
          printf("start\n");
          bufR[0] = '\0';
          if (byte == FLAG) {
            state = Flag_RCV;
            bufR[0] = FLAG;
          } else
            state = Start;

          break;

        case Flag_RCV:
          if (byte == FLAG)
            state = Flag_RCV;

          else if (byte == add2) {
            state = A_RCV;
            bufR[1] = add2;
          }

          else
            state = Start;

          break;

        case A_RCV:
          printf("a\n");
          if (byte == FLAG)
            state = Flag_RCV;

          else if (byte == DISC) {
            state = C_RCV;
            bufR[2] = DISC;
          }

          else
            state = Start;

          break;

        case C_RCV:
          printf("c\n");
          if (byte == FLAG)
            state = Flag_RCV;

          else if (byte == (bufR[1] ^ bufR[2])) {
            bufR[3] = bufR[1] ^ bufR[2];
            state = BCC_RCV;
          }

          else
            state = Start;

          break;

        case BCC_RCV:

          printf("bcc\n");
          if (byte == FLAG) {
            bufR[4] = FLAG;
            state = Stop;
            STOP = TRUE;
          }

          else
            state = Start;
          break;

        case Stop:
          printf("stop\n");
          STOP = TRUE;
          break;
        default:
          break;
        }
      }
      if (alarmEnabled) {
        printf("Received control word: ");
        alarmCount = 0;
        alarm(0);

        for (int i = 0; i < 5; i++) {
          printf("0x%02X ", bufR[i]);
        }
        if (bufR[2] != DISC) {
          printf("Did not receive disconnect byte\n");
        } else {
          break;
        }
      }
    }

    unsigned char bufS[5] = {0};

    bufS[0] = FLAG;
    bufS[1] = add2;
    bufS[2] = UA;
    bufS[3] = add2 ^ UA;
    bufS[4] = FLAG;
    sleep(0.2);
    writeBytesSerialPort(bufS, 5);

    printf("SENDING: ");
    for (int i = 0; i < 5; i++) {
      printf("0x%02X ", bufS[i]);
    }

    if (closeSerialPort() < 0) {
      perror("closeSerialPort");
    }
    printf("\nClosed the connection\n");
    return 0;
  } else {
    int nBytesBuf = 0;

    State state = Start;
    STOP = FALSE;
    while (STOP == FALSE) {

      unsigned char byte;
      int bytes = readByteSerialPort(&byte);
      nBytesBuf += bytes;

      switch (state) {
      case Start:
        printf("start\n");
        if (byte == FLAG)
          state = Flag_RCV;

        else
          state = Start;

        break;

      case Flag_RCV:
        printf("flag\n");
        if (byte == FLAG)
          state = Flag_RCV;

        else if (byte == add1)
          state = A_RCV;

        else
          state = Start;

        break;

      case A_RCV:
        printf("a\n");
        if (byte == FLAG)
          state = Flag_RCV;

        else if (byte == DISC)
          state = C_RCV;

        else
          state = Start;

        break;

      case C_RCV:
        printf("c\n");
        if (byte == FLAG)
          state = Flag_RCV;

        else if (byte == (DISC ^ add1))
          state = BCC_RCV;

        else
          state = Start;

        break;

      case BCC_RCV:

        printf("bcc\n");
        if (byte == FLAG) {
          state = Stop;
          STOP = TRUE;
        }

        else
          state = Start;
        break;

      case Stop:
        printf("stop\n");
        STOP = TRUE;
        break;
      default:
        break;
      }
    }
    unsigned char buf[BUF_SIZE] = {0};

    buf[0] = FLAG;
    buf[1] = add2;
    buf[2] = DISC;
    buf[3] = add2 ^ DISC;
    buf[4] = FLAG;

    state = Start;
    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1) {
      perror("sigaction");
      return 1;
    }

    printf("Alarm configured\n");

    alarmCount = 0;
    alarmEnabled = FALSE;
    while (alarmCount < info.nRetransmissions && !alarmEnabled) {
      int bytes = writeBytesSerialPort(buf, 5);
      printf("Sending control word: ");
      int i = 0;

      while (i < 5) {
        printf("0x%02X ", buf[i]);
        i++;
      }
      printf("%d bytes written to serial port\n", bytes);
      if (alarmEnabled == FALSE) {
        alarm(info.timeout);
        alarmEnabled = TRUE;
      }

      STOP = FALSE;
      unsigned char bufR[BUF_SIZE] = {0};
      while (STOP == FALSE) {
        if (!alarmEnabled) {
          break;
        }
        unsigned char byte;
        readByteSerialPort(&byte);

        switch (state) {
        case Start:
          printf("start\n");
          bufR[0] = '\0';
          if (byte == FLAG) {
            state = Flag_RCV;
            bufR[0] = FLAG;
          } else
            state = Start;

          break;

        case Flag_RCV:
          if (byte == FLAG)
            state = Flag_RCV;

          else if (byte == add2) {
            state = A_RCV;
            bufR[1] = add2;
          }

          else
            state = Start;

          break;

        case A_RCV:
          printf("a\n");
          if (byte == FLAG)
            state = Flag_RCV;

          else if (byte == UA) {
            state = C_RCV;
            bufR[2] = UA;
          }

          else
            state = Start;

          break;

        case C_RCV:
          printf("c\n");
          if (byte == FLAG)
            state = Flag_RCV;

          else if (byte == (UA ^ add2)) {
            bufR[3] = UA ^ add2;
            state = BCC_RCV;
          }

          else
            state = Start;

          break;

        case BCC_RCV:

          printf("bcc\n");
          if (byte == FLAG) {
            bufR[4] = FLAG;
            state = Stop;
            STOP = TRUE;
          }

          else
            state = Start;
          break;

        case Stop:
          printf("stop\n");
          STOP = TRUE;
          break;
        default:
          break;
        }
      }
      if (alarmEnabled) {
        printf("Received control word: ");
        alarmCount = 0;
        alarm(0);

        for (int i = 0; i < 5; i++) {
          printf("0x%02X ", bufR[i]);
        }
        break;
      }
    }
    if (closeSerialPort() < 0) {
      perror("closeSerialPort");
    }
    printf("\nClosed the connection\n");
    return 0;
  }
}
