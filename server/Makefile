VERSION = 0.2.1
SHELL = /bin/bash

CC      = /usr/bin/g++
CFLAGS  = -std=c++0x -Wall -Wno-unused-function -O3 -D_REENTRANT -DVERSION=\"$(VERSION)\" -I..
LDFLAGS = -lrt -lpthread -lboost_system -lboost_program_options -lvorbis -lvorbisenc -logg -lFLAC -lavahi-client -lavahi-common

OBJ = snapServer.o controlServer.o flacEncoder.o pcmEncoder.o oggEncoder.o serverSession.o publishAvahi.o pipeReader.o encoderFactory.o ../common/log.o ../message/pcmChunk.o ../message/sampleFormat.o
BIN = snapserver

all:	server

server: $(OBJ)
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
	cp ./snapserver /usr/sbin/snapserver; \
	cp ../init.scripts/systemd/snapserver.service /lib/systemd/system/snapserver.service; \
	systemctl daemon-reload; \
	systemctl enable snapserver; \
	systemctl start snapserver; \

installsysv:
	@echo using sysv; \
	cp ./snapserver /usr/sbin/snapserver; \
	cp ../init.scripts/sysv/snapserver /etc/init.d/snapserver; \
	update-rc.d snapserver defaults; \
	/etc/init.d/snapserver start; \


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
	@/etc/init.d/snapserver stop; \
	killall -9 snapserver; \
	rm -f /usr/sbin/snapserver; \
	rm -f /etc/init.d/snapserver; \
	update-rc.d -f snapserver remove; \

uninstallsystemd:
	@systemctl stop snapserver; \
	systemctl disable snapserver; \
	killall -9 snapserver; \
	rm -f /usr/sbin/snapserver; \
	rm -f /lib/systemd/system/snapserver.service; \
	systemctl daemon-reload; \