# Steps to run download.c

In order to execute and compile the file `download.c`, we created a Makefile with the following contents:

```Makefile
CC = gcc
CFLAGS = -Wall -Wextra -D_GNU_SOURCE -static-libgcc

SRC = src
BIN = bin

# Default URL (can be overridden via command line)
URL = ftp://demo:password@test.rebex.net/readme.txt    
#ftp://ftp.up.pt/pub/kodi/timestamp.txt
# make run_download URL="ftp://ftp.up.pt/pub/kodi/timestamp.txt"

.PHONY: all clean run_download

# $(BIN) as a dependency to ensure the folder exists
all: $(BIN) $(BIN)/getIP $(BIN)/clientTCP $(BIN)/download

# Rule to create the bin directory if it doesn't exist
$(BIN):
	mkdir -p $(BIN)

$(BIN)/download: $(SRC)/download.c
	$(CC) $(CFLAGS) -o $@ $^

$(BIN)/getIP: $(SRC)/getip.c
	$(CC) $(CFLAGS) -o $@ $^

$(BIN)/clientTCP: $(SRC)/clientTCP.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf $(BIN)/*

run_download: $(BIN)/download
	./$(BIN)/download $(URL)
```

The Makefile defines a default URL : `ftp://demo:password@test.rebex.net/readme.txt`

In order to run and compile `download.c`, we execute the following command:

```bash
make run_download
```

This compiles and executes `download.c` with the default URL, however it is possible to execute it with a personalized URL:

```bash
make run_download URL="<FTP Server to download file>"
```

