CC = gcc
CFLAGS = -Wall -Wextra -D_GNU_SOURCE

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