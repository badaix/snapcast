VERSION = 0.9.0
TARGET  = snapserver
SHELL = /bin/bash

ifdef DESTDIR
# dh_auto_install (Debian) sets this variable
  TARGET_DIR = $(DESTDIR)/usr
else
  TARGET_DIR ?= /usr
endif

CXXFLAGS += -std=c++0x -Wall -Wno-unused-function -O3 -DASIO_STANDALONE -DVERSION=\"$(VERSION)\" -I. -I.. -I../externals/asio/asio/include -I../externals/popl/include
OBJ       = snapServer.o config.o controlServer.o controlSession.o streamServer.o streamSession.o json/jsonrpc.o streamreader/streamUri.o streamreader/streamManager.o streamreader/pcmStream.o streamreader/pipeStream.o streamreader/fileStream.o encoder/encoderFactory.o encoder/flacEncoder.o encoder/pcmEncoder.o encoder/oggEncoder.o ../common/log.o ../common/sampleFormat.o ../message/pcmChunk.o

ifeq ($(ENDIAN), BIG)
CXXFLAGS += -DIS_BIG_ENDIAN
endif

ifeq ($(TARGET), OPENWRT)

STRIP     = echo
CXXFLAGS += -DNO_CPP11_STRING -DHAS_AVAHI -pthread
LDFLAGS   = -lvorbis -lvorbisenc -logg -lFLAC -lavahi-client -lavahi-common -latomic
OBJ      += publishZeroConf/publishAvahi.o 

else ifeq ($(TARGET), FREEBSD)

CXX       = /usr/local/bin/g++
STRIP     = echo
CXXFLAGS += -DNO_CPP11_STRING -DFREEBSD -DHAS_AVAHI -pthread
LDFLAGS   = -lrt -lvorbis -lvorbisenc -logg -lFLAC -lavahi-client -lavahi-common -static-libgcc -static-libstdc++
OBJ      += publishZeroConf/publishAvahi.o 

else ifeq ($(TARGET), MACOS)

CXX       = /usr/bin/g++
STRIP     = strip
CXXFLAGS += -DFREEBSD -DMACOS -DHAS_BONJOUR -Wno-deprecated -I/usr/local/include
LDFLAGS   = -lvorbis -lvorbisenc -logg -lFLAC -L/usr/local/lib 
OBJ      += publishZeroConf/publishBonjour.o

else

CXX       = /usr/bin/g++
STRIP     = strip
CXXFLAGS += -DHAS_AVAHI -pthread
LDFLAGS   = -lrt -lvorbis -lvorbisenc -logg -lFLAC -lavahi-client -lavahi-common -static-libgcc -static-libstdc++
OBJ      += publishZeroConf/publishAvahi.o 

endif


BIN = snapserver

all:	$(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)
	$(STRIP) $(BIN)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN) $(OBJ) *~

.PHONY: dpkg
#sudo apt-get install build-essential debhelper dh-make dh-systemd quilt fakeroot lintian
dpkg:
	dpkg-buildpackage -rfakeroot -b
	dh_clean

ifdef DESTDIR

install:
	install -D -g root -o root $(BIN) $(TARGET_DIR)/sbin/$(BIN)
	install -D -g root -o root $(BIN).1 $(TARGET_DIR)/share/man/man1/$(BIN).1

else ifeq ($(TARGET), FREEBSD)

install:
	echo BSD
	install -g wheel -o root -m 555 $(BIN) $(TARGET_DIR)/local/sbin/$(BIN)
	install -g wheel -o root -m 555 $(BIN).1 $(TARGET_DIR)/local/man/man1/$(BIN).1
	install -g wheel -o root -m 555 debian/$(BIN).bsd $(TARGET_DIR)/local/etc/rc.d/$(BIN)

else ifeq ($(TARGET), MACOS)

install:
	echo macOS
	install -g wheel -o root $(BIN) $(TARGET_DIR)/local/bin/$(BIN)
	install -g wheel -o root $(BIN).1 $(TARGET_DIR)/local/share/man/man1/$(BIN).1
	install -g wheel -o root debian/$(BIN).plist /Library/LaunchAgents/de.badaix.snapcast.$(BIN).plist
	launchctl load /Library/LaunchAgents/de.badaix.snapcast.$(BIN).plist

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
	cp debian/$(BIN).service /lib/systemd/system/$(BIN).service; \
	cp debian/$(BIN).default /etc/default/$(BIN); \
	systemctl daemon-reload; \
	systemctl enable $(BIN); \
	systemctl start $(BIN); \

installsysv:
	@echo using sysv; \
	cp debian/$(BIN).init /etc/init.d/$(BIN); \
	cp debian/$(BIN).default /etc/default/$(BIN); \
	update-rc.d $(BIN) defaults; \
	/etc/init.d/$(BIN) start; \

installbsd:
	@echo using bsd; \
	cp debian/$(BIN).bsd /usr/local/etc/rc.d/$(BIN); \


ifeq ($(TARGET), FREEBSD)

uninstall:
	@service $(BIN) stop; \
	killall -9 $(BIN); \
	rm -f $(TARGET_DIR)/local/sbin/$(BIN); \
	rm -f $(TARGET_DIR)/local/man/man1/$(BIN).1; \
	rm -f $(TARGET_DIR)/local/etc/rc.d/$(BIN); \

else ifeq ($(TARGET), MACOS)

uninstall:
	@launchctl unload /Library/LaunchAgents/de.badaix.snapcast.$(BIN).plist; \
	killall -9 $(BIN); \
	rm -f $(TARGET_DIR)/local/bin/$(BIN); \
	rm -f $(TARGET_DIR)/local/share/man/man1/$(BIN).1; \
	rm -f /Library/LaunchAgents/de.badaix.snapcast.$(BIN).plist; \

else

uninstall:
	rm -f $(TARGET_DIR)/share/man/man1/$(BIN).1
	@if [[ `systemctl` =~ -\.mount ]]; then \
		$(MAKE) uninstallsystemd; \
	elif [[ `/sbin/init --version` =~ upstart ]]; then \
		$(MAKE) uninstallsysv; \
	elif [[ -f /etc/init.d/cron && ! -h /etc/init.d/cron ]]; then \
		$(MAKE) uninstallsysv; \
	else \
		echo cannot tell; \
	fi; \

endif

uninstallsysv:
	@/etc/init.d/$(BIN) stop; \
	killall -9 $(BIN); \
	rm -f /usr/sbin/$(BIN); \
	rm -f /etc/init.d/$(BIN); \
	rm -f /etc/default/$(BIN); \
	update-rc.d -f $(BIN) remove; \

uninstallsystemd:
	@systemctl stop $(BIN); \
	systemctl disable $(BIN); \
	killall -9 $(BIN); \
	rm -f /usr/sbin/$(BIN); \
	rm -f /lib/systemd/system/$(BIN).service; \
	rm -f /etc/default/$(BIN); \
	systemctl daemon-reload; \

