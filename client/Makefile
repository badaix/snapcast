#    This file is part of snapcast
#   Copyright (C) 2014-2018  Johannes Pohl
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.

VERSION = 0.15.0
BIN = snapclient

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

# Simplify building debuggable executables 'make DEBUG=-g STRIP=echo'
DEBUG=-O3


CXXFLAGS += $(ADD_CFLAGS) -std=c++0x -Wall -Wno-unused-function $(DEBUG) -DHAS_FLAC -DHAS_OGG -DASIO_STANDALONE -DVERSION=\"$(VERSION)\" -I. -I.. -isystem ../externals/asio/asio/include -I../externals/popl/include -I../externals/aixlog/include -I../externals -I../common
LDFLAGS   = $(ADD_LDFLAGS) -logg -lFLAC
OBJ       = snapClient.o stream.o clientConnection.o timeProvider.o player/player.o decoder/pcmDecoder.o decoder/oggDecoder.o decoder/flacDecoder.o controller.o ../common/sampleFormat.o


ifneq (,$(TARGET))
CXXFLAGS += -D$(TARGET)
endif

ifeq ($(ENDIAN), BIG)
CXXFLAGS += -DIS_BIG_ENDIAN
endif

ifeq ($(TARGET), ANDROID)

CXX       = $(PROGRAM_PREFIX)clang++
STRIP     = $(PROGRAM_PREFIX)strip
CXXFLAGS += -pthread -fPIC -DHAS_TREMOR -DHAS_OPENSL -I$(NDK_DIR)/include
LDFLAGS   = -L$(NDK_DIR)/lib -pie -lvorbisidec -logg -lFLAC -lOpenSLES -latomic -llog -static-libstdc++
OBJ      += player/openslPlayer.o 

else ifeq ($(TARGET), OPENWRT)

STRIP     = echo
CXXFLAGS += -pthread -DNO_CPP11_STRING -DHAS_TREMOR -DHAS_ALSA -DHAS_AVAHI -DHAS_DAEMON
LDFLAGS  += -lasound -lvorbisidec -lavahi-client -lavahi-common -latomic
OBJ      += ../common/daemon.o player/alsaPlayer.o browseZeroConf/browseAvahi.o

else ifeq ($(TARGET), BUILDROOT)

CXXFLAGS += -pthread -DNO_CPP11_STRING -DHAS_TREMOR -DHAS_ALSA -DHAS_AVAHI -DHAS_DAEMON
LDFLAGS  += -lasound -lvorbisidec -lavahi-client -lavahi-common -latomic
OBJ      += ../common/daemon.o player/alsaPlayer.o browseZeroConf/browseAvahi.o

else ifeq ($(TARGET), MACOS)

CXX       = g++
STRIP     = strip
CXXFLAGS += -DHAS_COREAUDIO -DHAS_VORBIS -DFREEBSD -DHAS_BONJOUR -DHAS_DAEMON -I/usr/local/include -Wno-unused-local-typedef -Wno-deprecated
LDFLAGS  += -lvorbis -lFLAC -L/usr/local/lib -framework AudioToolbox -framework CoreAudio -framework CoreFoundation -framework IOKit
OBJ      += ../common/daemon.o player/coreAudioPlayer.o browseZeroConf/browseBonjour.o

else

CXX       = g++
STRIP     = strip
CXXFLAGS += -pthread -DHAS_VORBIS -DHAS_ALSA -DHAS_AVAHI -DHAS_DAEMON
LDFLAGS  += -lrt -lasound -lvorbis -lavahi-client -lavahi-common -static-libgcc -static-libstdc++ -latomic
OBJ      += ../common/daemon.o player/alsaPlayer.o browseZeroConf/browseAvahi.o

endif


all:	check-env $(BIN)

check-env:
ifeq ($(TARGET), ANDROID)
ifndef NDK_DIR
	$(error android NDK_DIR is not set)
endif
ifndef ARCH
	$(error ARCH is not set (arm, mips, x86))
endif
ifeq ($(ARCH), x86)
	$(eval PROGRAM_PREFIX:=$(NDK_DIR)/bin/i686-linux-android-)
else ifeq ($(ARCH), mips)
	$(eval CXXFLAGS:=$(CXXFLAGS) -DIS_BIG_ENDIAN)
	$(eval PROGRAM_PREFIX:=$(NDK_DIR)/bin/mipsel-linux-android-)
else
	$(eval CXXFLAGS:=$(CXXFLAGS) -march=armv7)
	$(eval PROGRAM_PREFIX:=$(NDK_DIR)/bin/arm-linux-androideabi-)
endif
endif

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)
	$(STRIP) $(BIN)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN) $(OBJ) *~ player/*.o

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
		useradd --gid audio --system snapclient; \
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

