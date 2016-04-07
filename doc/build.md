https://wiki.openwrt.org/doc/howto/buildroot.exigence

1. Do everything as non-root user
2. Issue all commands in the <buildroot dir> directory, e.g. ~/openwrt
<snapcast dir>

```
git clone git://git.openwrt.org/15.05/openwrt.git
```


```
cd openwrt
./scripts/feeds update -a
./scripts/feeds install -a
```


```
make menuconfig
make
```


Within the OpenWrt directory I'm linking to it, like this:
```
cd <buildroot dir>/package/sxx/snapcast
ln -s <snapcast dir>/openWrt/Makefile.openwrt Makefile
ln -s <snapcast dir> src

ls -l
Makefile -> <snapcast dir>/openWrt/Makefile.openwrt
src -> <snapcast dir>
```
...and build like this:
```
cd <buildroot dir>
make package/sxx/snapcast/clean V=s
make package/sxx/snapcast/compile -j1 V=s
```
