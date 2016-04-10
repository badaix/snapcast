VERSION = 0.5.0
TARGET  = snapclient
SHELL = /bin/bash

ifdef DESTDIR
# dh_auto_install (Debian) sets this variable
  TARGET_DIR = $(DESTDIR)/usr
else
  TARGET_DIR ?= /usr
endif


CXXFLAGS += $(ADD_CFLAGS) -std=c++0x -Wall -Wno-unused-function -O3 -pthread -DASIO_STANDALONE -DVERSION=\"$(VERSION)\" -I. -I.. -I../externals/asio/asio/include -I../externals/popl/include
OBJ       = snapClient.o stream.o clientConnection.o timeProvider.o player/player.o decoder/pcmDecoder.o decoder/flacDecoder.o controller.o ../message/pcmChunk.o ../common/log.o ../common/sampleFormat.o

ifeq ($(TARGET), ANDROID)

CXX      = $(NDK_DIR)/bin/arm-linux-androideabi-g++
STRIP	 = $(NDK_DIR)/bin/arm-linux-androideabi-strip
CXXFLAGS += -DANDROID -DNO_CPP11_STRING -fPIC -DHAS_OPENSL -I$(NDK_DIR)/include
LDFLAGS  = -L$(NDK_DIR)/lib -pie -lFLAC -lOpenSLES
OBJ      += player/openslPlayer.o

else ifeq ($(TARGET), OPENWRT)

STRIP	 = echo
CXXFLAGS += -DIS_BIG_ENDIAN -DNO_CPP11_STRING -DHAS_TREMOR -DHAS_ALSA -DHAS_AVAHI -DHAS_DAEMON
LDFLAGS  = -lasound -logg -lvorbisidec -lFLAC -lavahi-client -lavahi-common -latomic
OBJ      += player/alsaPlayer.o decoder/oggDecoder.o browseAvahi.o

else

CXX      = /usr/bin/g++
STRIP	 = strip
CXXFLAGS += -static-libgcc -static-libstdc++ -DHAS_OGG -DHAS_ALSA -DHAS_AVAHI -DHAS_DAEMON
LDFLAGS  = -lrt -lasound -logg -lvorbis -lFLAC -lavahi-client -lavahi-common
OBJ      += player/alsaPlayer.o decoder/oggDecoder.o browseAvahi.o

endif


BIN = snapclient


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

