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
BIN = snapserver

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

CXXFLAGS += $(ADD_CFLAGS) -std=c++17 -Wall -Wextra -Wpedantic -Wno-unused-function -DBOOST_ERROR_CODE_HEADER_ONLY -DHAS_FLAC -DHAS_OGG -DHAS_VORBIS -DHAS_VORBIS_ENC -DHAS_OPUS -DHAS_SOXR -DVERSION=\"$(VERSION)\" -I. -I.. -I../common
LDFLAGS  += $(ADD_LDFLAGS) -lvorbis -lvorbisenc -logg -lFLAC -lopus -lsoxr
OBJ       = snapserver.o server.o config.o control_server.o control_session_tcp.o control_session_http.o control_session_ws.o stream_server.o stream_session.o stream_session_tcp.o stream_session_ws.o streamreader/stream_uri.o streamreader/base64.o streamreader/stream_manager.o streamreader/pcm_stream.o streamreader/posix_stream.o streamreader/pipe_stream.o streamreader/file_stream.o streamreader/tcp_stream.o streamreader/process_stream.o streamreader/airplay_stream.o streamreader/meta_stream.o streamreader/librespot_stream.o streamreader/watchdog.o streamreader/control_error.o streamreader/stream_control.o streamreader/metadata.o streamreader/properties.o encoder/encoder_factory.o encoder/flac_encoder.o encoder/opus_encoder.o encoder/pcm_encoder.o encoder/null_encoder.o encoder/ogg_encoder.o ../common/sample_format.o ../common/resampler.o

ifneq (,$(TARGET))
CXXFLAGS += -D$(TARGET)
endif

ifeq ($(ENDIAN), BIG)
CXXFLAGS += -DIS_BIG_ENDIAN
endif

ifeq ($(TARGET), ANDROID)

CXX       = $(NDK_DIR)/bin/arm-linux-androideabi-g++
CXXFLAGS += -pthread -DNO_CPP11_STRING -fPIC -I$(NDK_DIR)/include
LDFLAGS  += -L$(NDK_DIR)/lib -pie -llog -latomic

else ifeq ($(TARGET), OPENWRT)

CXXFLAGS += -DNO_CPP11_STRING -DHAS_AVAHI -DHAS_DAEMON -DHAS_ALSA -pthread
LDFLAGS  += -lavahi-client -lavahi-common -latomic
OBJ      += ../common/daemon.o publishZeroConf/publish_avahi.o streamreader/alsa_stream.o

else ifeq ($(TARGET), BUILDROOT)

CXXFLAGS += -DHAS_AVAHI -DHAS_DAEMON -DHAS_ALSA -pthread
LDFLAGS  += -lrt -lavahi-client -lavahi-common
OBJ      += publishZeroConf/publish_avahi.o streamreader/alsa_stream.o

else ifeq ($(TARGET), FREEBSD)

CXX       = g++
CXXFLAGS += -DFREEBSD -DNO_CPP11_STRING -DHAS_AVAHI -DHAS_DAEMON -pthread
LDFLAGS  += -lrt -lavahi-client -lavahi-common -latomic
OBJ      += ../common/daemon.o publishZeroConf/publish_avahi.o 

else ifeq ($(TARGET), MACOS)

CXX       = g++
CXXFLAGS += -DFREEBSD -DMACOS -DHAS_BONJOUR -DHAS_DAEMON -Wno-deprecated -I/usr/local/include
LDFLAGS  += -L/usr/local/lib -framework CoreFoundation -framework IOKit
OBJ      += ../common/daemon.o publishZeroConf/publish_bonjour.o

else

CXX       = g++
CXXFLAGS += -DHAS_AVAHI -DHAS_DAEMON -DHAS_ALSA -pthread
LDFLAGS  += -lrt -lasound -lavahi-client -lavahi-common -latomic
OBJ      += ../common/daemon.o publishZeroConf/publish_avahi.o streamreader/alsa_stream.o

endif

ifdef HAS_EXPAT
CXXFLAGS += -DHAS_EXPAT
LDFLAGS  += -lexpat
endif


all:	$(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN) $(OBJ) *~

ifdef DESTDIR

install:
	$(MAKE) installfiles

else ifeq ($(TARGET), FREEBSD)

install:
	echo BSD
	install -s -g wheel -o root -m 555 $(BIN) $(TARGET_DIR)/local/bin/$(BIN)
	install -g wheel -o root -m 555 $(BIN).1 $(TARGET_DIR)/local/man/man1/$(BIN).1
	install -g wheel -o root -m 555 etc/$(BIN).bsd $(TARGET_DIR)/local/etc/rc.d/$(BIN)
	install -g wheel -o root etc/$(BIN).conf /etc/$(BIN).conf
	install -g wheel -o root -m 644 etc/index.html -Dt /usr/share/snapserver/
	for file in etc/snapweb/*\.*; do install -g wheel -o root -m 644 $${file} -Dt "/usr/share/snapserver/snapweb/"; done
	for file in etc/snapweb/3rd-party/*\.*; do install -g wheel -o root -m 644 $${file} -Dt "/usr/share/snapserver/snapweb/3rd-party/"; done

else ifeq ($(TARGET), MACOS)

install:
	echo macOS
	install -s -g wheel -o root $(BIN) $(TARGET_DIR)/local/bin/$(BIN)
	install -g wheel -o root $(BIN).1 $(TARGET_DIR)/local/share/man/man1/$(BIN).1
	install -g wheel -o root etc/$(BIN).plist /Library/LaunchAgents/de.badaix.snapcast.$(BIN).plist
	install -g wheel -o root etc/$(BIN).conf /etc/$(BIN).conf
	install -g wheel -o root -m 644 etc/index.html -Dt /usr/share/snapserver/
	for file in etc/snapweb/*\.*; do install -g wheel -o root -m 644 $${file} -Dt "/usr/share/snapserver/snapweb/"; done
	for file in etc/snapweb/3rd-party/*\.*; do install -g wheel -o root -m 644 $${file} -Dt "/usr/share/snapserver/snapweb/3rd-party/"; done
	launchctl load /Library/LaunchAgents/de.badaix.snapcast.$(BIN).plist

else

install:
	$(MAKE) adduser
	$(MAKE) installfiles
	install -g snapserver -o snapserver -d /var/run/$(BIN)

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
	install -g root -o root etc/$(BIN).conf /etc/$(BIN).conf;
	install -g root -o root -m 644 etc/index.html -Dt /usr/share/snapserver/
	for file in etc/snapweb/*\.*; do install -g root -o root -m 644 $${file} -Dt "/usr/share/snapserver/snapweb/"; done
	for file in etc/snapweb/3rd-party/*\.*; do install -g root -o root -m 644 $${file} -Dt "/usr/share/snapserver/snapweb/3rd-party/"; done

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
	sh ../debian/snapserver.postinst configure

ifeq ($(TARGET), FREEBSD)

uninstall:
	@service $(BIN) stop; \
	killall -9 $(BIN); \
	rm -f $(TARGET_DIR)/local/bin/$(BIN); \
	rm -f $(TARGET_DIR)/local/man/man1/$(BIN).1; \
	rm -f $(TARGET_DIR)/local/etc/rc.d/$(BIN); \
 	rm -f /etc/$(BIN).conf; \
 	rm -rf /var/lib/snapserver; \
	rm -rf /usr/share/snapserver; \

else ifeq ($(TARGET), MACOS)

uninstall:
	@launchctl unload /Library/LaunchAgents/de.badaix.snapcast.$(BIN).plist; \
	killall -9 $(BIN); \
	rm -f $(TARGET_DIR)/local/bin/$(BIN); \
	rm -f $(TARGET_DIR)/local/share/man/man1/$(BIN).1; \
	rm -f /Library/LaunchAgents/de.badaix.snapcast.$(BIN).plist; \
 	rm -f /etc/$(BIN).conf; \
	rm -rf /var/lib/snapserver; \
	rm -rf /usr/share/snapserver; \

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
 	rm -f /etc/$(BIN).conf; \
	rm -rf /var/run/$(BIN); \
	rm -rf /var/lib/snapserver; \
	rm -rf /usr/share/snapserver; \
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
	sh ../debian/snapserver.postrm purge

