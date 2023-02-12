Name:           snapcast 
Release:        1
License:        GPL-3.0 
Group:          Productivity/Multimedia/Sound/Players
Summary:        Snapcast is a multi-room time-synced client-server audio player
Url:            https://github.com/badaix/snapcast 
Source0:        snapcast.tar.gz 
Source1:        snapserver.service
Source2:        snapserver.default
Source3:        snapclient.service
Source4:        snapclient.default
BuildRequires:  alsa-lib-devel 
BuildRequires:  pulseaudio-libs-devel
BuildRequires:  avahi-devel 
BuildRequires:  libvorbis-devel
BuildRequires:  flac-devel
BuildRequires:  gcc-c++ 
BuildRequires:  boost-devel >= 1.74
BuildRequires:  opus-devel
BuildRequires:  soxr-devel
BuildRequires:  zlib-devel
#Requires(post): %fillup_prereq
Requires(pre):  pwdutils
BuildRequires:  systemd
#%if 0%{?suse_version} >= 1210
BuildRequires:  systemd-rpm-macros
#%endif
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
%{!?_reversion: %define _reversion "" }
%{!?_version:   %define _version "0.0.0" }
Version:        %{_version}

%description 
Snapcast is a multi-room client-server audio player, where all clients are time synchronized with the server to play perfectly synced audio. It is not a standalone player, but an extension that turns your existing audio player into a Sonos-like multi-room solution. The server's audio input is a named pipe /tmp/snapfifo. All data that is fed into this file will be send to the connected clients. One of the most generic ways to use Snapcast is in conjunction with the music player daemon (MPD) or Mopidy, which can be configured to use a named pipe as audio output.

%package -n snapserver
Summary:        Snapcast server 

%description -n snapserver
Snapcast is a multi-room client-server audio player, where all clients are
time synchronized with the server to play perfectly synced audio. It's not a
standalone player, but an extension that turns your existing audio player into
a Sonos-like multi-room solution.  The server's audio input is a named 
pipe `/tmp/snapfifo`. All data that is fed into this file will be send to 
the connected clients. One of the most generic ways to use Snapcast is in 
conjunction with the music player daemon, MPD, or Mopidy, which can be configured 
to use a named pipe as audio output.
This package contains the server to which clients connect.

%package -n snapclient 
Summary:        Snapcast client 

%description -n snapclient
Snapcast is a multi-room client-server audio player, where all clients are
time synchronized with the server to play perfectly synced audio. It's not a
standalone player, but an extension that turns your existing audio player into
a Sonos-like multi-room solution. 
This package contains the client which connects to the server and plays the audio.

%prep 
%setup -q -n %{name}

%build 
%cmake -DWERROR=ON -DBUILD_TESTS=OFF -DREVISION=%{_reversion}
%cmake_build --parallel 2

%install
%cmake_install

chmod 755 %{buildroot}%{_datadir}/snapserver/plug-ins/meta_mpd.py
install -D -m 0644 %{SOURCE3} %{buildroot}%{_unitdir}/snapclient.service
install -D -m 0644 %{SOURCE1} %{buildroot}%{_unitdir}/snapserver.service
install -D -m 0644 %{SOURCE4} %{buildroot}/etc/default/snapserver.default
install -D -m 0644 %{SOURCE2} %{buildroot}/etc/default/snapclient.default

#install -D -m 0644 snapserver.service     $RPM_BUILD_ROOT/usr/lib/systemd/system/snapserver.service
#install -D -m 0644 snapclient.service     $RPM_BUILD_ROOT/usr/lib/systemd/system/snapclient.service

#install -m 644 %{SOURCE2} %{buildroot}%{_fillupdir}
#install -m 644 %{SOURCE4} %{buildroot}%{_fillupdir}


%pre -n snapclient
getent passwd snapclient >/dev/null || %{_sbindir}/useradd --user-group --system --groups audio snapclient 
%if 0%{?suse_version}
%service_add_pre snapclient.service
%endif

%pre -n snapserver
mkdir -p %{_sharedstatedir}/snapserver
getent passwd snapserver >/dev/null || %{_sbindir}/useradd --user-group --system --home-dir %{_sharedstatedir}/snapserver snapserver 
chown snapserver %{_sharedstatedir}/snapserver
chgrp snapserver %{_sharedstatedir}/snapserver
%if 0%{?suse_version}
%service_add_pre snapserver.service
%endif

%post -n snapclient
#%{fillup_only -n snapclient snapclient}
#%service_add_post snapclient.service
%if 0%{?suse_version}
%service_add_post snapclient.service
%else
%systemd_post snapclient.service
%endif

%post -n snapserver 
#%{fillup_only -n snapserver snapserver}
#%service_add_post snapserver.service
%if 0%{?suse_version}
%service_add_post snapserver.service
%else
%systemd_post snapserver.service
%endif

%preun -n snapclient 
%if 0%{?suse_version}
%service_del_preun snapclient.service
%else
%systemd_preun snapclient.service
%endif

%preun -n snapserver
%if 0%{?suse_version}
%service_del_preun snapserver.service
%else
%systemd_preun snapserver.service
%endif

%postun -n snapclient 
%if 0%{?suse_version}
%service_del_postun snapclient.service
%else
%systemd_postun_with_restart snapclient.service
%endif
if [ $1 -eq 0 ]; then
   userdel --force snapclient 2> /dev/null; true
fi

%postun -n snapserver
%if 0%{?suse_version}
%service_del_postun snapserver.service
%else
%systemd_postun_with_restart snapserver.service
%endif

#%files 
#%license LICENSE
#%doc README.md TODO.md doc/*

%files -n snapserver
%{_bindir}/snapserver
#%{_mandir}/man1/snapserver.1.*
%{_datadir}
%config(noreplace) /usr/etc/snapserver.conf
%config(noreplace) /etc/default/snapserver.default
%{_unitdir}/snapserver.service
#%{_fillupdir}/sysconfig.snapserver

%files -n snapclient
%{_bindir}/snapclient
%{_mandir}/man1/snapclient.1.*
%config(noreplace) /etc/default/snapclient.default
%{_unitdir}/snapclient.service
#%{_fillupdir}/sysconfig.snapclient

%changelog
