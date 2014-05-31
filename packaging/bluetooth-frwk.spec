%if %{_repository}=="wearable"
%define _dumpdir /opt/etc/dump.d/module.d
%endif
Name:       bluetooth-frwk
Summary:    Bluetooth framework for BlueZ and Obexd. This package is Bluetooth framework based on BlueZ and Obexd stack.
Version:    0.2.57
Release:    1
Group:      TO_BE/FILLED_IN
License:    Apache License, Version 2.0
Source0:    %{name}-%{version}.tar.gz
%if %{_repository}=="wearable"
Source1:    bluetooth-frwk-wearable.service
%endif
%if %{_repository}=="mobile"
Source1:    bluetooth-frwk-mobile.service
%endif
Requires: sys-assert
Requires: dbus
Requires: syspopup
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gio-2.0)
%if %{_repository}=="wearable"
BuildRequires:  pkgconfig(gio-unix-2.0)
%endif
%if %{_repository}=="mobile"
BuildRequires:  pkgconfig(libsystemd-daemon)
BuildRequires:  python-xml
%endif
BuildRequires:  pkgconfig(syspopup-caller)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(libxml-2.0)
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(utilX)
BuildRequires:  pkgconfig(capi-network-tethering)
BuildRequires:  pkgconfig(libprivilege-control)
BuildRequires:  pkgconfig(status)
BuildRequires:  pkgconfig(alarm-service)
BuildRequires:  pkgconfig(notification)
BuildRequires:  pkgconfig(security-server)
BuildRequires:  pkgconfig(capi-content-mime-type)
BuildRequires:  cmake

Requires(post): vconf
Requires(postun): eglibc

%description
Bluetooth framework for BlueZ and Obexd. This package is Bluetooth framework based on BlueZ and Obexd stack.
 This package contains API set for BT GAP, BT SDP, and BT RFCOMM.


%package devel
Summary:    Bluetooth framework for BlueZ and Obexd
Group:      TO_BE/FILLED
Requires:   %{name} = %{version}-%{release}

%description devel
This package is development files for Bluetooth framework based on BlueZ and Obexd stack.
This package contains API set for BT GAP, BT SDP, and BT RFCOMM.

%package service
Summary:    Bluetooth Service daemon
Group:      TO_BE/FILLED
Requires:   %{name} = %{version}-%{release}

%description service
This package is Bluetooth Service daemon to manage BT services.

%package core
Summary:    Bluetooth Core daemon
Group:      TO_BE/FILLED
Requires:   %{name} = %{version}-%{release}

%description core
This package is Bluetooth core daemon to manage activation / deactivation.

%prep
%setup -q


%build
%if %{_repository}=="wearable"
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"
export CFLAGS+=" -fpie -D__ENABLE_GDBUS__ -DGIOOUT_Q_WRITE -DRFCOMM_DIRECT"
export LDFLAGS+=" -Wl,--rpath=/usr/lib -Wl,--as-needed -Wl,--unresolved-symbols=ignore-in-shared-libs -pie"
cd wearable
%elseif %{_repository}=="mobile"
%ifarch x86_64
export CFLAGS+="   -Wall -g -fvisibility=hidden -fPIC -D__ENABLE_GDBUS__"
export LDFLAGS+=" -Wl,--rpath=%{_libdir} -Wl,--as-needed -Wl,--unresolved-symbols=ignore-in-shared-libs" 
%else
export CFLAGS+=" -fpie -D__ENABLE_GDBUS__"
export LDFLAGS+=" -Wl,--rpath=%{_libdir} -Wl,--as-needed -Wl,--unresolved-symbols=ignore-in-shared-libs -pie"
%endif
cd mobile
%endif
cmake . -DCMAKE_INSTALL_PREFIX=/usr

make

%install
rm -rf %{buildroot}

%if %{_repository}=="wearable"
cd wearable
%elseif %{_repository}=="mobile"
cd mobile
%endif

%make_install

%if %{_repository}=="wearable"
install -D -m 0644 LICENSE.APLv2 %{buildroot}%{_datadir}/license/bluetooth-frwk
install -D -m 0644 LICENSE.APLv2 %{buildroot}%{_datadir}/license/bluetooth-frwk-core
install -D -m 0644 LICENSE.APLv2 %{buildroot}%{_datadir}/license/bluetooth-frwk-devel
install -D -m 0644 LICENSE.APLv2 %{buildroot}%{_datadir}/license/bluetooth-frwk-service

mkdir -p %{buildroot}%{_libdir}/systemd/system/graphical.target.wants
install -m 0644 %SOURCE1 %{buildroot}%{_libdir}/systemd/system/bluetooth-frwk.service
ln -s ../bluetooth-frwk.service %{buildroot}%{_libdir}/systemd/system/graphical.target.wants/bluetooth-frwk.service

mkdir -p %{buildroot}%{_dumpdir}
install -m 0755 bluetooth_log_dump.sh %{buildroot}%{_dumpdir}
%elseif %{_repository}=="mobile"
mkdir -p %{buildroot}%{_libdir}/systemd/user
mkdir -p %{buildroot}%{_libdir}/systemd/user/tizen-middleware.target.wants
install -m 0644 %SOURCE1 %{buildroot}%{_libdir}/systemd/user/bluetooth-frwk.service
ln -s ../bluetooth-frwk.service %{buildroot}%{_libdir}/systemd/user/tizen-middleware.target.wants/bluetooth-frwk.service

mkdir -p %{buildroot}/usr/share/license
cp LICENSE.APLv2 %{buildroot}/usr/share/license/%{name}
cp LICENSE.APLv2 %{buildroot}/usr/share/license/%{name}-devel
cp LICENSE.APLv2 %{buildroot}/usr/share/license/%{name}-service
cp LICENSE.APLv2 %{buildroot}/usr/share/license/%{name}-core
%endif

%post
%if %{_repository}=="wearable"
vconftool set -tf int db/bluetooth/status "1" -g 6520 -s system::vconf_network
vconftool set -tf int file/private/bt-service/flight_mode_deactivated "0" -g 6520 -i -s system::vconf_network
vconftool set -tf int file/private/bt-service/powersaving_mode_deactivated "0" -g 6520 -i -s system::vconf_network
vconftool set -tf int file/private/bt-service/bt_off_due_to_timeout "0" -g 6520 -i -s bt-service
vconftool set -tf string memory/bluetooth/sco_headset_name "" -g 6520 -i -s system::vconf_network
vconftool set -tf int memory/bluetooth/device "0" -g 6520 -i -s system::vconf_network
vconftool set -tf int memory/bluetooth/btsco "0" -g 6520 -i -s system::vconf_network
vconftool set -tf bool memory/bluetooth/dutmode "0" -g 6520 -i -s system::vconf_network

%post service
mkdir -p %{_sysconfdir}/systemd/default-extra-dependencies/ignore-units.d/
ln -s %{_libdir}/systemd/system/bluetooth-frwk.service %{_sysconfdir}/systemd/default-extra-dependencies/ignore-units.d/
%elseif %{_repository}=="mobile"
vconftool set -tf int db/bluetooth/status "0" -g 6520
vconftool set -tf int file/private/bt-service/flight_mode_deactivated "0" -g 6520 -i
vconftool set -tf int file/private/bt-service/powersaving_mode_deactivated "0" -g 6520 -i
vconftool set -tf int file/private/bt-service/bt_off_due_to_timeout "0" -g 6520 -i
vconftool set -tf string memory/bluetooth/sco_headset_name "" -g 6520 -i
vconftool set -tf int memory/bluetooth/device "0" -g 6520 -i
vconftool set -tf int memory/bluetooth/btsco "0" -g 6520 -i
%endif

%postun -p /sbin/ldconfig

%files
%defattr(-, root, root)
%{_libdir}/libbluetooth-api.so.*
%if %{_repository}=="wearable"
%{_datadir}/license/bluetooth-frwk
%{_libdir}/systemd/system/graphical.target.wants/bluetooth-frwk.service
%{_libdir}/systemd/system/bluetooth-frwk.service
%elseif %{_repository}=="mobile"
/usr/share/license/%{name}
%endif

%files devel
%defattr(-, root, root)
%{_includedir}/bt-service/bluetooth-api.h
%{_includedir}/bt-service/bluetooth-hid-api.h
%{_includedir}/bt-service/bluetooth-audio-api.h
%{_includedir}/bt-service/bluetooth-telephony-api.h
%{_includedir}/bt-service/bluetooth-media-control.h
%{_libdir}/pkgconfig/bluetooth-api.pc
%{_libdir}/libbluetooth-api.so
%if %{_repository}=="wearable"
%{_includedir}/bt-service/bluetooth-scmst-api.h
%{_datadir}/license/bluetooth-frwk-devel
%elseif %{_repository}=="mobile"
/usr/share/license/%{name}-devel
%endif

%files service
%if %{_repository}=="wearable"
%manifest wearable/bluetooth-frwk.manifest
%elseif %{_repository}=="mobile"
%manifest mobile/bluetooth-frwk.manifest
%endif
%defattr(-, root, root)
%{_sysconfdir}/rc.d/init.d/bluetooth-frwk-service
%{_datadir}/dbus-1/services/org.projectx.bt.service
%{_bindir}/bt-service
%if %{_repository}=="wearable"
%{_bindir}/bluetooth-frwk-test
%{_bindir}/bluetooth-gatt-test
%{_bindir}/bluetooth-hf-test
%{_bindir}/bluetooth-advertising-test
%attr(0666,-,-) /opt/var/lib/bluetooth/auto-pair-blacklist
%{_datadir}/license/bluetooth-frwk-service
%{_dumpdir}/bluetooth_log_dump.sh
%elseif %{_repository}=="mobile"
%{_libdir}/systemd/user/tizen-middleware.target.wants/bluetooth-frwk.service
%{_libdir}/systemd/user/bluetooth-frwk.service
/etc/smack/accesses.d/bluetooth-frwk-service.rule
%attr(0666,-,-) /opt/var/lib/bluetooth/auto-pair-blacklist
/usr/share/license/%{name}-service
%endif

%files core
%if %{_repository}=="wearable"
%manifest wearable/bluetooth-frwk-core.manifest
%endif
%defattr(-, root, root)
%{_datadir}/dbus-1/services/org.projectx.bt_core.service
%{_bindir}/bt-core
%if %{_repository}=="wearable"
%{_datadir}/license/bluetooth-frwk-core
%elseif %{_repository}=="mobile"
/usr/share/license/%{name}-core
%endif
