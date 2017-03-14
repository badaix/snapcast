VERSION = 0.11.1
TARGET  = snapclient
ifeq ($(TARGET), FREEBSD)
SHELL   = /usr/local/bin/bash
else
SHELL   = /bin/bash
endif

ifdef DESTDIR
# dh_auto_install (Debian) sets this variable
  TARGET_DIR = $(DESTDIR)/usr
else
  TARGET_DIR ?= /usr
endif


CXXFLAGS += $(ADD_CFLAGS) -std=c++0x -Wall -Wno-unused-function -O3 -DASIO_STANDALONE -DVERSION=\"$(VERSION)\" -I. -I.. -isystem ../externals/asio/asio/include -I../externals/popl/include
OBJ       = snapClient.o stream.o clientConnection.o timeProvider.o player/player.o decoder/pcmDecoder.o decoder/oggDecoder.o decoder/flacDecoder.o controller.o ../message/pcmChunk.o ../common/log.o ../common/sampleFormat.o

ifeq ($(ENDIAN), BIG)
CXXFLAGS += -DIS_BIG_ENDIAN
endif

ifeq ($(TARGET), ANDROID)

CXX       = $(NDK_DIR)/bin/arm-linux-androideabi-g++
STRIP     = $(NDK_DIR)/bin/arm-linux-androideabi-strip
CXXFLAGS += -pthread -DANDROID -DNO_CPP11_STRING -fPIC -DHAS_OGG -DHAS_TREMOR -DHAS_OPENSL -I$(NDK_DIR)/include
LDFLAGS   = -L$(NDK_DIR)/lib -pie -lvorbisidec -logg -lFLAC -lOpenSLES 
OBJ      += player/openslPlayer.o 

else ifeq ($(TARGET), OPENWRT)

STRIP     = echo
CXXFLAGS += -pthread -DNO_CPP11_STRING -DHAS_OGG -DHAS_TREMOR -DHAS_ALSA -DHAS_AVAHI -DHAS_DAEMON
LDFLAGS   = -lasound -lvorbisidec -logg -lFLAC -lavahi-client -lavahi-common -latomic
OBJ      += player/alsaPlayer.o browseZeroConf/browseAvahi.o

else ifeq ($(TARGET), BUILDROOT)

CXXFLAGS += -pthread -DNO_CPP11_STRING -DHAS_OGG -DHAS_TREMOR -DHAS_ALSA -DHAS_AVAHI -DHAS_DAEMON
LDFLAGS  += -lasound -lvorbisidec -logg -lFLAC -lavahi-client -lavahi-common -latomic
OBJ      += player/alsaPlayer.o browseZeroConf/browseAvahi.o

else ifeq ($(TARGET), MACOS)

CXX       = g++
STRIP     = strip
CXXFLAGS += -DHAS_OGG -DHAS_VORBIS -DHAS_COREAUDIO -DFREEBSD -DMACOS -DHAS_BONJOUR -DHAS_DAEMON -I/usr/local/include -Wno-unused-local-typedef -Wno-deprecated
LDFLAGS   = -logg -lvorbis -lFLAC -L/usr/local/lib -framework AudioToolbox -framework CoreFoundation
OBJ      += player/coreAudioPlayer.o browseZeroConf/browseBonjour.o

else

CXX       = g++
STRIP     = strip
CXXFLAGS += -pthread -DHAS_OGG -DHAS_VORBIS -DHAS_ALSA -DHAS_AVAHI -DHAS_DAEMON
LDFLAGS   = -lrt -lasound -logg -lvorbis -lFLAC -lavahi-client -lavahi-common -static-libgcc -static-libstdc++
OBJ      += player/alsaPlayer.o browseZeroConf/browseAvahi.o

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
	dpkg-buildpackage -rfakeroot -b -uc -us
	dh_clean

ifdef DESTDIR

install:
	$(MAKE) installfiles

else ifeq ($(TARGET), MACOS)

install:
	echo macOS
	install -g wheel -o root $(BIN) $(TARGET_DIR)/local/bin/$(BIN)
	install -g wheel -o root $(BIN).1 $(TARGET_DIR)/local/share/man/man1/$(BIN).1
	install -g wheel -o root debian/$(BIN).plist /Library/LaunchAgents/de.badaix.snapcast.$(BIN).plist
	launchctl load /Library/LaunchAgents/de.badaix.snapcast.$(BIN).plist

else

install:
	$(MAKE) adduser
	$(MAKE) installfiles
	install -g audio -o snapclient -d /var/run/$(BIN)

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

installfiles:
	install -D -g root -o root $(BIN) $(TARGET_DIR)/bin/$(BIN)
	install -D -g root -o root $(BIN).1 $(TARGET_DIR)/share/man/man1/$(BIN).1

installsystemd:
	@echo using systemd; \
	cp debian/$(BIN).service /lib/systemd/system/$(BIN).service; \
	cp -n debian/$(BIN).default /etc/default/$(BIN); \
	systemctl daemon-reload; \
	systemctl enable $(BIN); \
	systemctl start $(BIN); \

installsysv:
	@echo using sysv; \
	cp debian/$(BIN).init /etc/init.d/$(BIN); \
	cp -n debian/$(BIN).default /etc/default/$(BIN); \
	update-rc.d $(BIN) defaults; \
	/etc/init.d/$(BIN) start; \

adduser:
	@if ! getent passwd snapclient >/dev/null; then \
		useradd --gid audio --system --home-dir /var/lib/snapclient snapclient; \
	fi; \

ifeq ($(TARGET), MACOS)

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
	rm -rf /var/run/$(BIN)
	$(MAKE) deluser

endif

uninstallsysv:
	@/etc/init.d/$(BIN) stop; \
	killall -9 $(BIN); \
	rm -f /usr/bin/$(BIN); \
	rm -f /etc/init.d/$(BIN); \
	rm -f /etc/default/$(BIN); \
	update-rc.d -f $(BIN) remove; \

uninstallsystemd:
	@systemctl stop $(BIN); \
	systemctl disable $(BIN); \
	killall -9 $(BIN); \
	rm -f /usr/bin/$(BIN); \
	rm -f /lib/systemd/system/$(BIN).service; \
	rm -f /etc/default/$(BIN); \
	systemctl daemon-reload; \

deluser:
	@userdel --force snapclient > /dev/null || true; \

