VERSION = 0.01
CC      = /usr/bin/g++
CFLAGS  = -std=gnu++0x -Wall -Wno-unused-function -D_REENTRANT -DVERSION=\"$(VERSION)\" -I..
LDFLAGS = -lrt -lpthread -lboost_system -lboost_program_options -lvorbis -lvorbisenc -logg

OBJ = snapServer.o controlServer.o pcmEncoder.o oggEncoder.o serverSession.o ../common/log.o ../message/pcmChunk.o ../message/sampleFormat.o
BIN = snapserver

all:	server

server: $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)
	strip $(BIN)
	
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN) $(OBJ) *~

