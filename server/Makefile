VERSION = 0.4.0
TARGET  = snapserver
SHELL = /bin/bash

ifdef DESTDIR
# dh_auto_install (Debian) sets this variable
  TARGET_DIR = $(DESTDIR)/usr
else
  TARGET_DIR ?= /usr
endif

CXX     = /usr/bin/g++
CFLAGS  = -std=c++0x -Wall -Wno-unused-function -O3 -pthread -DVERSION=\"$(VERSION)\" -I..
LDFLAGS = -lrt -lboost_system -lboost_program_options -lvorbis -lvorbisenc -logg -lFLAC -lavahi-client -lavahi-common

OBJ = snapServer.o controlServer.o encoder/encoderFactory.o encoder/flacEncoder.o encoder/pcmEncoder.o encoder/oggEncoder.o serverSession.o publishAvahi.o pipeReader.o ../common/log.o ../message/pcmChunk.o ../message/sampleFormat.o
BIN = snapserver

all:	$(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)
	strip $(BIN)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN) $(OBJ) *~

.PHONY: dpkg
#sudo apt-get install build-essential debhelper dh-make dh-systemd quilt fakeroot lintian
dpkg:
	cp ../init.scripts/sysv/$(BIN) debian/$(BIN).init
	cp ../init.scripts/systemd/$(BIN).service debian/$(BIN).service
	dpkg-buildpackage -rfakeroot -b
	dh_clean
	rm -f debian/$(BIN).service
	rm -f debian/$(BIN).init

ifdef DESTDIR

install:
	install -D -g root -o root $(BIN) $(TARGET_DIR)/sbin/$(BIN)
	install -D -g root -o root $(BIN).1 $(TARGET_DIR)/share/man/man1/$(BIN).1

else

install:
	install -D -g root -o root $(BIN) $(TARGET_DIR)/sbin/$(BIN)
	install -D -g root -o root $(BIN).1 $(TARGET_DIR)/share/man/man1/$(BIN).1
	@if [[ `systemctl` =~ -\.mount ]]; then \
		$(MAKE) installsystemd; \
	elif [[ `/sbin/init --version` =~ upstart ]]; then \
		$(MAKE) installsysv; \
	elif [[ -f /etc/init.d/cron && ! -h /etc/init.d/cron ]]; then \
		$(MAKE) installsysv; \
	else \
		echo cannot tell; \
	fi; \

endif

installsystemd:
	@echo using systemd; \
	cp ../init.scripts/systemd/$(BIN).service /lib/systemd/system/$(BIN).service; \
	systemctl daemon-reload; \
	systemctl enable $(BIN); \
	systemctl start $(BIN); \

installsysv:
	@echo using sysv; \
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