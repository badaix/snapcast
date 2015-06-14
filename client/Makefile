VERSION = 0.2
TARGET  = snapclient
ifdef DESTDIR
# dh_auto_install (Debian) sets this variable
  TARGET_DIR = $(DESTDIR)/usr
else
  TARGET_DIR ?= /usr/local
endif
CC      = /usr/bin/g++
CFLAGS  = -std=gnu++0x -static-libgcc -static-libstdc++ -Wall -O3 -D_REENTRANT -DVERSION=\"$(VERSION)\" -I..
LDFLAGS = -lrt -lpthread -lboost_system -lboost_program_options -lasound -logg -lvorbis -lvorbisenc -lFLAC -lavahi-client -lavahi-common

OBJ = snapClient.o stream.o alsaPlayer.o clientConnection.o timeProvider.o oggDecoder.o pcmDecoder.o flacDecoder.o controller.o browseAvahi.o ../message/pcmChunk.o ../common/log.o ../message/sampleFormat.o
BIN = snapclient

all:	$(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)
	strip $(BIN)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN) $(OBJ) *~

install: $(TARGET) uninstall
#	install -D -g root -o root $(TARGET) $(TARGET_DIR)/sbin/$(TARGET)
	@cp ./snapclient /usr/sbin/snapclient
	cp ../init.d/snapclient /etc/init.d/snapclient; \
	update-rc.d snapclient defaults; \
	/etc/init.d/snapclient start; \

uninstall:
	@/etc/init.d/snapclient stop; \
	killall -9 snapclient; \
	rm -f /usr/sbin/snapclient; \
	rm -f /etc/init.d/snapclient; \
	update-rc.d -f snapclient remove; \

