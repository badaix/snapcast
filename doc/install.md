# Install Linux packages

Snapcast packages are available for several Linux distributions:

- [Debian](#debian)
- [OpenWrt](#openwrt)
- [Alpine Linux](#alpine-linux)
- [Archlinux](#archlinux)
- [Void Linux](#void-linux)

## Debian

For Debian (and Debian-based systems, such as Ubuntu, Linux Mint, elementary OS) download the package for your CPU architecture from the [latest release page](https://github.com/badaix/snapcast/releases/latest).

e.g. for Raspberry Pi `snapclient_0.x.x_armhf.deb`, for laptops `snapclient_0.x.x_amd64.deb`

### using apt 1.1 or later

    sudo apt install </path/to/snapclient_0.x.x_[arch].deb>

or

    sudo apt install </path/to/snapserver_0.x.x_[arch].deb>

### using dpkg

Install the package:

    sudo dpkg -i </path/to/snapclient_0.x.x_[arch].deb>

or

    sudo dpkg -i </path/to/snapserver_0.x.x_[arch].deb>

Install missing dependencies:

    sudo apt-get -f install

## OpenWrt

On OpenWrt do:

    opkg install snapclient_0.x.x_ar71xx.ipk

## Alpine Linux

On Alpine Linux do:

    apk add snapcast

Or, for just the client:

    apk add snapcast-client

Or, for just the server:

    apk add snapcast-server

## Gentoo Linux

On Gentoo Linux do:

    emerge --ask media-sound/snapcast

## Archlinux

On Archlinux, Snapcast is available through the AUR.  To install, use your favorite AUR helper, or do:

    git clone https://aur.archlinux.org/snapcast
    cd snapcast
    makepkg -si

## Void Linux

To install the client:

    # xbps-install snapclient

To install the server:

    # xbps-install snapserver
