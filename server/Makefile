VERSION = 0.2
CC      = /usr/bin/g++
CFLAGS  = -std=gnu++0x -Wall -Wno-unused-function -O3 -D_REENTRANT -DVERSION=\"$(VERSION)\" -I..
LDFLAGS = -lrt -lpthread -lboost_system -lboost_program_options -lvorbis -lvorbisenc -logg -lFLAC -lavahi-client -lavahi-common

OBJ = snapServer.o controlServer.o flacEncoder.o pcmEncoder.o oggEncoder.o serverSession.o publishAvahi.o ../common/log.o ../message/pcmChunk.o ../message/sampleFormat.o
BIN = snapserver

all:	server

server: $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)
	strip $(BIN)
	
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN) $(OBJ) *~

install: uninstall
	@cp ./snapserver /usr/sbin/snapserver; \
	cp ../init.d/snapserver /etc/init.d/snapserver; \
	update-rc.d snapserver defaults; \
	/etc/init.d/snapserver start; \

uninstall:
	@/etc/init.d/snapserver stop; \
	killall -9 snapserver; \
	rm -f /usr/sbin/snapserver; \
	rm -f /etc/init.d/snapserver; \
	update-rc.d -f snapserver remove; \
	
