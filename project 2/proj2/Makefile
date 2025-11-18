CC = gcc
CFLAGS = -Wall

SRC = src/
BIN = bin/

URL = ftp://rcom:rcom123@ftp.up.pt/pub/asdasd/ficheiro.txt

.PHONY: all
all: $(BIN)/getIP $(BIN)/clientTCP $(BIN)/download

$(BIN)/download: $(SRC)/download.c
	$(CC) $(CFLAGS) -o $@ $^

$(BIN)/getIP: $(SRC)/getip.c
	$(CC) $(CFLAGS) -o $@ $^

$(BIN)/clientTCP: $(SRC)/clientTCP.c
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f $(BIN)/getIP
	rm -f $(BIN)/clientTCP
	rm -f $(BIN)/download

.PHONY: run_download
run_download: $(BIN)/download
	./$(BIN)/download $(URL)