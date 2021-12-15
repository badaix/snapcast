#    This file is part of snapcast
#   Copyright (C) 2014-2021  Johannes Pohl
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

VERSION = 0.26.0
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

DEBUG ?= 0
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2
endif

ifneq ($(SANITIZE), )
	CXXFLAGS += -fsanitize=$(SANITIZE) -g
	LDFLAGS += -fsanitize=$(SANITIZE)
endif

CXXFLAGS += $(ADD_CFLAGS) -std=c++17 -Wall -Wextra -Wpedantic -Wno-unused-function -DBOOST_ERROR_CODE_HEADER_ONLY -DHAS_FLAC -DHAS_OGG -DHAS_OPUS -DHAS_SOXR -DVERSION=\"$(VERSION)\" -I. -I.. -I../common
LDFLAGS  += $(ADD_LDFLAGS) -logg -lFLAC -lopus -lsoxr
OBJ       = snapclient.o stream.o client_connection.o time_provider.o player/player.o player/file_player.o decoder/pcm_decoder.o decoder/ogg_decoder.o decoder/flac_decoder.o decoder/opus_decoder.o controller.o ../common/sample_format.o ../common/resampler.o


ifneq (,$(TARGET))
CXXFLAGS += -D$(TARGET)
endif

ifeq ($(ENDIAN), BIG)
CXXFLAGS += -DIS_BIG_ENDIAN
endif

ifeq ($(TARGET), ANDROID)

CXX       = $(PROGRAM_PREFIX)clang++
CXXFLAGS += -pthread -fPIC -DHAS_TREMOR -DHAS_OPENSL -DHAS_OBOE -I$(TOOLCHAIN)/sysroot/usr/include -I$(TOOLCHAIN)/sysroot/$(NDK_TARGET)/usr/local/include
LDFLAGS   = -L$(TOOLCHAIN)/sysroot/$(NDK_TARGET)/usr/local/lib -pie -lvorbisidec -logg -lFLAC -lopus -lsoxr -lOpenSLES -loboe -latomic -llog -static-libstdc++
OBJ      += player/opensl_player.o player/oboe_player.o

else ifeq ($(TARGET), OPENWRT)

CXXFLAGS += -pthread -DNO_CPP11_STRING -DHAS_TREMOR -DHAS_ALSA -DHAS_AVAHI -DHAS_DAEMON
LDFLAGS  += -lasound -lvorbisidec -lavahi-client -lavahi-common -latomic
OBJ      += ../common/daemon.o player/alsa_player.o browseZeroConf/browse_avahi.o

else ifeq ($(TARGET), BUILDROOT)

CXXFLAGS += -pthread -DNO_CPP11_STRING -DHAS_TREMOR -DHAS_ALSA -DHAS_AVAHI -DHAS_DAEMON
LDFLAGS  += -lasound -lvorbisidec -lavahi-client -lavahi-common -latomic
OBJ      += ../common/daemon.o player/alsa_player.o browseZeroConf/browse_avahi.o

else ifeq ($(TARGET), MACOS)

CXX       = g++
CXXFLAGS += -DHAS_COREAUDIO -DHAS_VORBIS -DFREEBSD -DMACOS -DHAS_BONJOUR -DHAS_DAEMON -I/usr/local/include -Wno-unused-local-typedef -Wno-deprecated
LDFLAGS  += -lvorbis -lFLAC -L/usr/local/lib -framework AudioToolbox -framework CoreAudio -framework CoreFoundation -framework IOKit
OBJ      += ../common/daemon.o player/coreaudio_player.o browseZeroConf/browse_bonjour.o

else

CXX       = g++
CXXFLAGS += -pthread -DHAS_VORBIS -DHAS_ALSA -DHAS_PULSE -DHAS_AVAHI -DHAS_DAEMON
LDFLAGS  += -lrt -lasound -lpulse -lvorbis -lavahi-client -lavahi-common -latomic
OBJ      += ../common/daemon.o player/alsa_player.o player/pulse_player.o browseZeroConf/browse_avahi.o

endif


all:	check-env $(BIN)

check-env:
ifeq ($(TARGET), ANDROID)
	$(eval TOOLCHAIN:=$(NDK_DIR)/toolchains/llvm/prebuilt/linux-x86_64)
ifndef NDK_DIR
	$(error android NDK_DIR is not set)
endif
ifndef ARCH
	$(error ARCH is not set (arm, aarch64, x86))
endif
ifeq ($(ARCH), x86)
	$(eval NDK_TARGET:=x86_64-linux-android)
	$(eval API:=21)
else ifeq ($(ARCH), arm)
	$(eval NDK_TARGET:=armv7a-linux-androideabi)
	$(eval API:=16)
	$(eval CXXFLAGS:=$(CXXFLAGS) -march=armv7)
else ifeq ($(ARCH), aarch64)
	$(eval NDK_TARGET:=aarch64-linux-android)
	$(eval API:=21)
endif
	$(eval PROGRAM_PREFIX:=$(TOOLCHAIN)/bin/$(NDK_TARGET)$(API)-)
endif

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN) $(OBJ) *~ player/*.o

ifdef DESTDIR

install:
	$(MAKE) installfiles

else ifeq ($(TARGET), MACOS)

install:
	echo macOS
	install -s -g wheel -o root $(BIN) $(TARGET_DIR)/local/bin/$(BIN)
	install -g wheel -o root $(BIN).1 $(TARGET_DIR)/local/share/man/man1/$(BIN).1
	install -g wheel -o root etc/$(BIN).plist /Library/LaunchAgents/de.badaix.snapcast.$(BIN).plist
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
	install -s -D -g root -o root $(BIN) $(TARGET_DIR)/bin/$(BIN)
	install -D -g root -o root $(BIN).1 $(TARGET_DIR)/share/man/man1/$(BIN).1

installsystemd:
	@echo using systemd; \
	cp ../debian/$(BIN).service /lib/systemd/system/$(BIN).service; \
	cp -n ../debian/$(BIN).default /etc/default/$(BIN); \
	systemctl daemon-reload; \
	systemctl enable $(BIN); \
	systemctl start $(BIN); \

installsysv:
	@echo using sysv; \
	cp ../debian/$(BIN).init /etc/init.d/$(BIN); \
	cp -n ../debian/$(BIN).default /etc/default/$(BIN); \
	update-rc.d $(BIN) defaults; \
	/etc/init.d/$(BIN) start; \

adduser:
	sh ../debian/snapclient.postinst configure

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
	sh ../debian/snapclient.postrm purge

