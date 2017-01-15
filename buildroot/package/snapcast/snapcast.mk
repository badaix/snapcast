################################################################################
#
# snapcast
#
################################################################################

SNAPCAST_VERSION = master
SNAPCAST_SITE = https://github.com/badaix/snapcast
SNAPCAST_SITE_METHOD = git
SNAPCAST_DEPENDENCIES = libogg alsa-lib # libstdcpp libavahi-client libatomic libflac libvorbisidec
SNAPCAST_LICENSE = GPLv3
SNAPCAST_LICENSE_FILES = COPYING

# http://lists.busybox.net/pipermail/buildroot/2013-March/069811.html
define SNAPCAST_EXTRACT_CMDS
	rm -rf $(@D)
	(git clone --depth 1 $(SNAPCAST_SITE) $(@D) && \
	cd $(@D)/externals && \
	git submodule update --init --recursive)
	touch $(@D)/.stamp_downloaded
endef

define SNAPCAST_BUILD_CMDS
	$(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)/client TARGET=BUILDROOT
endef

define SNAPCAST_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 -D $(@D)/client/snapclient $(TARGET_DIR)/usr/sbin/snapclient
	$(INSTALL) -m 0755 -D $(SNAPCAST_PKGDIR)/S99snapclient $(TARGET_DIR)/etc/init.d/S99snapclient
endef

$(eval $(generic-package))
