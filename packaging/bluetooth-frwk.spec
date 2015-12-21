%define _dumpdir /opt/etc/dump.d/module.d
%define _varlibdir /opt/var/lib

Name:       bluetooth-frwk
Summary:    Bluetooth framework for BlueZ and Obexd. This package is Bluetooth framework based on BlueZ and Obexd stack.
Version:    0.2.149
Release:    1
Group:      TO_BE/FILLED_IN
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz

Requires: sys-assert
Requires: dbus
Requires: syspopup
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(gio-unix-2.0)
BuildRequires:  pkgconfig(syspopup-caller)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(libxml-2.0)
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(utilX)
BuildRequires:  pkgconfig(capi-network-connection)
BuildRequires:  pkgconfig(alarm-service)
BuildRequires:  pkgconfig(security-server)
BuildRequires:  pkgconfig(capi-content-mime-type)
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(pkgmgr)
BuildRequires:  pkgconfig(journal)
BuildRequires:  pkgconfig(eventsystem)
BuildRequires:	pkgconfig(libprivilege-control)
BuildRequires:	pkgconfig(libsoup-2.4)

%if "%{?tizen_profile_name}" == "mobile"
BuildRequires:  pkgconfig(capi-network-tethering)
%endif

BuildRequires:  cmake

Requires(post): vconf
Requires(postun): eglibc
Requires: psmisc

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

%package httpproxy
Summary:    Bluetooth HTTP Proxy Service daemon
Group:      TO_BE/FILLED
Requires:   %{name} = %{version}-%{release}

%description httpproxy
This package is Bluetooth HTTP Proxy Service daemon

%package core
Summary:    Bluetooth Core daemon
Group:      TO_BE/FILLED
Requires:   %{name} = %{version}-%{release}

%description core
This package is Bluetooth core daemon to manage activation / deactivation.

%package test
Summary:    Bluetooth test application
Group:      TO_BE/FILLED
Requires:   %{name} = %{version}-%{release}

%description test
This package is Bluetooth test application.

%prep
%setup -q


%build
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"

%if "%{?tizen_profile_name}" == "mobile"
export CFLAGS="$CFLAGS -DTIZEN_BT_FLIGHTMODE_ENABLED -DTIZEN_NETWORK_TETHERING_ENABLE -DTIZEN_TELEPHONY_ENABLED -DTIZEN_BT_PAN_NAP_ENABLE"
%if "%{?tizen_target_name}" == "Z300H" || "%{?tizen_target_name}" == "TM1"
%define _servicefile packaging/bluetooth-frwk-mobile-sprd.service
%else
%define _servicefile packaging/bluetooth-frwk-mobile.service
%endif
%define _servicedir multi-user.target.wants
%endif

%if "%{?tizen_profile_name}" == "wearable"
export CFLAGS="$CFLAGS -DTIZEN_WEARABLE"
%define _servicefile packaging/bluetooth-frwk-wearable.service
%define _servicedir multi-user.target.wants
%endif

%if "%{?tizen_profile_name}" == "common"
export CFLAGS="$CFLAGS -DUSB_BLUETOOTH"
%define _servicefile packaging/bluetooth-frwk.service
%define _servicedir starter.target.wants
%endif

%if "%{?tizen_profile_name}" == "tv"
export CFLAGS="$CFLAGS -DUSB_BLUETOOTH"
%define _servicefile packaging/bluetooth-frwk.service
%define _servicedir starter.target.wants
%endif

export LDFLAGS+=" -Wl,--rpath=/usr/lib -Wl,--as-needed -Wl,--unresolved-symbols=ignore-in-shared-libs -pie"

#export CFLAGS+=" -fpie -DRFCOMM_DIRECT "
export CFLAGS+=" -fpie -DRFCOMM_DIRECT -DGATT_NO_RELAY"
cmake . -DCMAKE_INSTALL_PREFIX=/usr

make

%cmake \
%if "%{?tizen_profile_name}" == "wearable"
	-DTIZEN_WEARABLE=YES \
%else
	-DTIZEN_WEARABLE=NO \
%endif

%install
rm -rf %{buildroot}
%make_install

install -D -m 0644 LICENSE %{buildroot}%{_datadir}/license/bluetooth-frwk
install -D -m 0644 LICENSE %{buildroot}%{_datadir}/license/bluetooth-frwk-service
install -D -m 0644 LICENSE %{buildroot}%{_datadir}/license/bluetooth-frwk-httpproxy
install -D -m 0644 LICENSE %{buildroot}%{_datadir}/license/bluetooth-frwk-devel

mkdir -p %{buildroot}%{_libdir}/systemd/system/%{_servicedir}
install -m 0644 %{_servicefile} %{buildroot}%{_libdir}/systemd/system/bluetooth-frwk.service
ln -s ../bluetooth-frwk.service %{buildroot}%{_libdir}/systemd/system/%{_servicedir}/bluetooth-frwk.service

mkdir -p %{buildroot}%{_dumpdir}
install -m 0755 bluetooth_log_dump.sh %{buildroot}%{_dumpdir}

install -D -m 0644 bt-core/bluetooth-frwk-core.rule %{buildroot}%{_sysconfdir}/smack/accesses.d/bluetooth-frwk-core.rule

#%post
#%if "%{?tizen_profile_name}" == "wearable"
#vconftool set -tf int db/bluetooth/status "1" -g 5000 -s tizen::vconf::public::r::platform::rw
#%else
#vconftool set -tf int db/bluetooth/status "0" -g 5000 -s tizen::vconf::public::r::platform::rw
#%endif
#vconftool set -tf int db/bluetooth/lestatus "0" -g 5000 -s tizen::vconf::public::r::platform::rw
#vconftool set -tf int file/private/bt-core/flight_mode_deactivated "0" -g 5000 -i -s tizen::vconf::platform::rw
#vconftool set -tf int file/private/bt-core/powersaving_mode_deactivated "0" -g 5000 -i -s tizen::vconf::platform::rw
#vconftool set -tf int file/private/bt-service/bt_off_due_to_timeout "0" -g 5000 -i -s tizen::vconf::platform::rw
#vconftool set -tf string memory/bluetooth/sco_headset_name "" -g 5000 -i -s tizen::vconf::platform::rw
#vconftool set -tf int memory/bluetooth/device "0" -g 5000 -i -s tizen::vconf::platform::rw
#vconftool set -tf bool memory/bluetooth/dutmode "0" -g 5000 -i -s tizen::vconf::platform::rw

%post service
mkdir -p %{_sysconfdir}/systemd/default-extra-dependencies/ignore-units.d/
ln -sf %{_libdir}/systemd/system/bluetooth-frwk.service %{_sysconfdir}/systemd/default-extra-dependencies/ignore-units.d/

%postun -p /sbin/ldconfig

%files
%defattr(-, root, root)
%{_libdir}/libbluetooth-api.so.*
%{_datadir}/license/bluetooth-frwk
%{_libdir}/systemd/system/%{_servicedir}/bluetooth-frwk.service
%{_libdir}/systemd/system/bluetooth-frwk.service

%files devel
%defattr(-, root, root)
%{_includedir}/bt-service/bluetooth-api.h
%{_includedir}/bt-service/bluetooth-hid-api.h
%{_includedir}/bt-service/bluetooth-audio-api.h
%{_includedir}/bt-service/bluetooth-telephony-api.h
%{_includedir}/bt-service/bluetooth-media-control.h
%{_includedir}/bt-service/bluetooth-scmst-api.h
%{_includedir}/bt-service/bluetooth-ipsp-api.h
%{_libdir}/pkgconfig/bluetooth-api.pc
%{_libdir}/libbluetooth-api.so
%{_datadir}/license/bluetooth-frwk-devel

%files service
%manifest bluetooth-frwk.manifest
%defattr(-, root, root)
%{_sysconfdir}/rc.d/init.d/bluetooth-frwk-service
%{_datadir}/dbus-1/system-services/org.projectx.bt.service
%{_bindir}/bt-service
%{_bindir}/bluetooth-frwk-test
%{_bindir}/bluetooth-gatt-test
%{_bindir}/bluetooth-advertising-test
%{_varlibdir}/bluetooth
%{_prefix}/etc/bluetooth
%attr(0666,-,-) %{_varlibdir}/bluetooth/auto-pair-blacklist
%attr(0666,-,-) %{_prefix}/etc/bluetooth/stack_info
%{_dumpdir}/bluetooth_log_dump.sh
%{_datadir}/license/bluetooth-frwk-service

%files httpproxy
%manifest bluetooth-httpproxy.manifest
%defattr(-, root, root)
%{_bindir}/bt-httpproxy
%{_datadir}/license/bluetooth-frwk-httpproxy
%{_datadir}/dbus-1/system-services/org.projectx.httpproxy.service

%files core
%manifest bluetooth-frwk-core.manifest
%defattr(-, root, root)
%{_datadir}/dbus-1/system-services/org.projectx.bt_core.service
%{_bindir}/bt-core
%{_sysconfdir}/smack/accesses.d/bluetooth-frwk-core.rule

%files test
%manifest bluetooth-frwk-test.manifest
%defattr(-, root, root)
%{_bindir}/bluetooth-frwk-test
%{_bindir}/bluetooth-gatt-test
%{_bindir}/bluetooth-advertising-test
