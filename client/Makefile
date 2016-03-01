VERSION = 0.5.0-beta-1
TARGET  = snapclient
SHELL = /bin/bash

ifdef DESTDIR
# dh_auto_install (Debian) sets this variable
  TARGET_DIR = $(DESTDIR)/usr
else
  TARGET_DIR ?= /usr
endif

# MIPS
# export STAGING_DIR=/home/johannes/Develop/openwrt/staging_dir
# TOOLCHAIN_DIR=$(STAGING_DIR)/toolchain-mips_34kc_gcc-4.8-linaro_uClibc-0.9.33.2
# LDCFLAGS=$(TOOLCHAIN_DIR)/usr/lib
# LD_LIBRARY_PATH=$(TOOLCHAIN_DIR)/usr/lib
# PATH=$(TOOLCHAIN_DIR)/bin:$PATH
# CXX     = $(TOOLCHAIN_DIR)/bin/mips-openwrt-linux-g++

ifdef ANDROID
# Android NDK setup: (http://developer.android.com/ndk/guides/standalone_toolchain.html)
# 1. Download NDK: http://dl.google.com/android/ndk/android-ndk-r10e-linux-x86_64.bin
# 2. Extract to: /home/johannes/Develop/android-ndk-r10e
# 3. Setup toolchain:
#    i.  cd /home/johannes/Develop/android-ndk-r10e/build/tools
#    ii. ./make-standalone-toolchain.sh --arch=arm --platform=android-14 --install-dir=/home/johannes/Develop/android-toolchain-arm-14 --ndk-dir=/home/johannes/Develop/android-ndk-r10e --system=linux-x86_64
# build with:
#    make ANDROID=1
CXX     = /home/johannes/Develop/android-toolchain-arm-14/bin/arm-linux-androideabi-g++
CFLAGS  = $(ADD_CFLAGS) -std=c++0x -Wall -Wno-unused-function -fPIC -O3 -pthread -DASIO_STANDALONE -DANDROID -DVERSION=\"$(VERSION)\" -I.. -I../externals/asio/asio/include -I../externals/popl/include -I/home/johannes/Develop/build/arm/include
#-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16
LDFLAGS = -L/home/johannes/Develop/build/arm/lib -pie -lFLAC -lOpenSLES
OBJ     = snapClient.o stream.o clientConnection.o timeProvider.o player/player.o player/openslPlayer.o decoder/pcmDecoder.o decoder/flacDecoder.o controller.o ../message/pcmChunk.o ../common/log.o ../message/sampleFormat.o
else
CXX     = /usr/bin/g++
CFLAGS  = -std=c++0x -Wall -Wno-unused-function -O3 -pthread -DASIO_STANDALONE -DVERSION=\"$(VERSION)\" -I.. -I../externals/asio/asio/include -I../externals/popl/include
LDFLAGS = -lrt -lasound -logg -lvorbis -lFLAC -lavahi-client -lavahi-common
OBJ     = snapClient.o stream.o clientConnection.o timeProvider.o player/player.o player/alsaPlayer.o decoder/oggDecoder.o decoder/pcmDecoder.o decoder/flacDecoder.o controller.o browseAvahi.o ../message/pcmChunk.o ../common/log.o ../message/sampleFormat.o
endif

BIN = snapclient


all:	$(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)
#	strip $(BIN)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

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

