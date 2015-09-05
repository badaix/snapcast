VERSION = 0.3.90
TARGET  = snapserver
SHELL = /bin/bash

CC      = /usr/bin/g++
CFLAGS  = -std=c++0x -Wall -Wno-unused-function -O3 -pthread -DVERSION=\"$(VERSION)\" -I..
LDFLAGS = -lrt -lboost_system -lboost_program_options -lvorbis -lvorbisenc -logg -lFLAC -lavahi-client -lavahi-common

OBJ = snapServer.o config.o controlServer.o controlSession.o streamServer.o jsonrpc.o encoder/encoderFactory.o encoder/flacEncoder.o encoder/pcmEncoder.o encoder/oggEncoder.o clientSession.o publishAvahi.o pipeReader.o ../common/log.o ../message/pcmChunk.o ../message/sampleFormat.o
BIN = snapserver

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