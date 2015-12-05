VERSION = 0.3.90
TARGET  = snapclient
SHELL = /bin/bash

#CXX     = /home/johannes/Develop/android-ndk-r10e-arm-21/bin/arm-linux-androideabi-g++
#CFLAGS  = -std=c++0x -Wall -Wno-unused-function -O3 -pthread -DASIO_STANDALONE -DVERSION=\"$(VERSION)\" -I.. -I../externals/asio/asio/include -I../externals/tclap/include -I../externals/ezOptionParser -I/home/johannes/Develop/build/arm/include -L/home/johannes/Develop/build/arm/lib

CXX     = /usr/bin/g++
CFLAGS  = -std=c++0x -Wall -Wno-unused-function -O3 -pthread -DASIO_STANDALONE -DVERSION=\"$(VERSION)\" -I.. -I../externals/asio/asio/include -I../externals/tclap/include -I../externals/ezOptionParser
LDFLAGS = -lrt -lasound -logg -lvorbis -lvorbisenc -lFLAC -lavahi-client -lavahi-common

OBJ = snapClient.o stream.o clientConnection.o timeProvider.o player/player.o player/alsaPlayer.o decoder/oggDecoder.o decoder/pcmDecoder.o decoder/flacDecoder.o controller.o browseAvahi.o ../message/pcmChunk.o ../common/log.o ../message/sampleFormat.o
BIN = snapclient

all:	$(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)
	strip $(BIN)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

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
	cp ./$(BIN) /usr/sbin/$(BIN); \
	cp ../init.scripts/systemd/$(BIN).service /lib/systemd/system/$(BIN).service; \
	systemctl daemon-reload; \
	systemctl enable $(BIN); \
	systemctl start $(BIN); \

installsysv:
	@echo using sysv; \
	cp ./$(BIN) /usr/sbin/$(BIN); \
	cp ../init.scripts/sysv/$(BIN) /etc/init.d/$(BIN); \
	update-rc.d $(BIN) defaults; \
	/etc/init.d/$(BIN) start; \


uninstall:
	@if [[ `systemctl` =~ -\.mount ]]; then \
		$(MAKE) uninstallsystemd; \
	elif [[ `/sbin/init --version` =~ upstart ]]; then \
		$(MAKE) uninstallsysv; \
	elif [[ -f /etc/init.d/cron && ! -h /etc/init.d/cron ]]; then \
		$(MAKE) uninstallsysv; \
	else \
		echo cannot tell; \
	fi; \

uninstallsysv:
	@/etc/init.d/$(BIN) stop; \
	killall -9 $(BIN); \
	rm -f /usr/sbin/$(BIN); \
	rm -f /etc/init.d/$(BIN); \
	update-rc.d -f $(BIN) remove; \

uninstallsystemd:
	@systemctl stop $(BIN); \
	systemctl disable $(BIN); \
	killall -9 $(BIN); \
	rm -f /usr/sbin/$(BIN); \
	rm -f /lib/systemd/system/$(BIN).service; \
	systemctl daemon-reload; \

