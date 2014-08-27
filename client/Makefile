VERSION = 0.01
CC      = /usr/bin/g++
CFLAGS  = -std=gnu++0x -Wall -Wno-unused-function -O3 -D_REENTRANT -DVERSION=\"$(VERSION)\" -I..
LDFLAGS = -lrt -lpthread -lboost_system -lboost_program_options -lasound

OBJ = snapClient.o stream.o player.o receiver.o ../common/chunk.o ../common/log.o ../common/sampleFormat.o
BIN = snapclient

all:	client

client: $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN) $(OBJ)
