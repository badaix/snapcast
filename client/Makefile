VERSION = 0.2.1
TARGET  = snapclient
SHELL = /bin/bash

CC      = /usr/bin/g++
CFLAGS  = -std=c++0x -static-libgcc -static-libstdc++ -Wall -Wno-unused-function -O3 -D_REENTRANT -DVERSION=\"$(VERSION)\" -I..
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
	@if [[ `systemctl` =~ -\.mount ]]; then \
		$(MAKE) installsystemd; \
	elif [[ `/sbin/init --version` =~ upstart ]]; then \
		$(MAKE) installsysv; \
	elif [[ -f /etc/init.d/cron && ! -h /etc/init.d/cron ]]; then \
		$(MAKE) installsysv; \
	else \
		echo cannot tell; \
	fi; \


installsystemd:
	@echo using systemd; \
	cp ./snapclient /usr/sbin/snapclient; \
	cp ../init.scripts/systemd/snapclient.service /lib/systemd/system/snapclient.service; \
	systemctl daemon-reload; \
	systemctl enable snapclient; \
	systemctl start snapclient; \

installsysv:
	@echo using sysv; \
	cp ./snapclient /usr/sbin/snapclient; \
	cp ../init.scripts/sysv/snapclient /etc/init.d/snapclient; \
	update-rc.d snapclient defaults; \
	/etc/init.d/snapclient start; \


uninstall:
	@if [[ `systemctl` =~ -\.mount ]]; then \
		$(MAKE) uninstallsystemd; \
	elif [[ `/sbin/init --version` =~ upstart ]]; then \
		echo upstart => sysv; \
		$(MAKE) uninstallsysv; \
	elif [[ -f /etc/init.d/cron && ! -h /etc/init.d/cron ]]; then \
		$(MAKE) uninstallsysv; \
	else \
		echo cannot tell; \
	fi; \

uninstallsysv:
	@/etc/init.d/snapclient stop; \
	killall -9 snapclient; \
	rm -f /usr/sbin/snapclient; \
	rm -f /etc/init.d/snapclient; \
	update-rc.d -f snapclient remove; \

uninstallsystemd:
	@systemctl stop snapclient; \
	systemctl disable snapclient; \
	killall -9 snapclient; \
	rm -f /usr/sbin/snapclient; \
	rm -f /lib/systemd/system/snapclient.service; \
	systemctl daemon-reload; \

