VERSION = 0.01
CC      = /usr/bin/g++
CFLAGS  = -std=gnu++0x -Wall -Wno-unused-function -O3 -D_REENTRANT -DVERSION=\"$(VERSION)\" -I..
LDFLAGS = -lrt -lpthread -lboost_system -lboost_program_options -lasound -logg -lvorbis -lvorbisenc

OBJ = snapClient.o stream.o alsaPlayer.o clientConnection.o timeProvider.o oggDecoder.o pcmDecoder.o controller.o ../message/pcmChunk.o ../common/log.o ../message/sampleFormat.o
BIN = snapclient

all:	client

client: $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)
	strip $(BIN)
	
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN) $(OBJ) *~
