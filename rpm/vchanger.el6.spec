#
#  Spec file for vchanger - RHEL 6
#
Summary: A virtual autochanger for Bacula
Name: vchanger
Version: 1.0.0
Release: 1.el6
License: GPLv2
Group: System Environment/Daemons
Source: http://sourceforge.net/projects/vchanger/files/vchanger/%{version}/vchanger-%{version}.tar.gz
URL: http://vchanger.sourceforge.net
Vendor: Josh Fisher.
Packager: Josh Fisher <jfisher@jaybus.com>
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires(post): /bin/mkdir, /bin/chown, /usr/bin/getent, /usr/sbin/useradd, /usr/sbin/groupadd

%description
Vchanger implements a virtual autochanger for use with the Bacula open source
network backup system. Vchanger emulates a magazine-based tape autoloader
using disk file system mountpoints as virtual magazines and the files in
each virtual magazine as virtual tape volumes. Vchanger is primarily
designed to use an unlimited number of removable disk drives as an easily
scalable virtual autochanger and allow seamless removal of backups for
offsite storage. 

%prep
%setup -q -n %{name}

%build
CFLAGS="%{optflags} -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE" \
%configure
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p ${RPM_BUILD_ROOT}%{_bindir}
mkdir -p ${RPM_BUILD_ROOT}%{_sysconfdir}/sysconfig
mkdir -p ${RPM_BUILD_ROOT}%{_sysconfdir}/%{name}
mkdir -p ${RPM_BUILD_ROOT}%{_libexecdir}/%{name}
mkdir -p ${RPM_BUILD_ROOT}%{_mandir}/man5
mkdir -p ${RPM_BUILD_ROOT}%{_mandir}/man8
mkdir -p ${RPM_BUILD_ROOT}%{_docdir}/%{name}-%{version}
mkdir -m 0770 -p ${RPM_BUILD_ROOT}%{_localstatedir}/spool/%{name}
mkdir -m 0755 -p ${RPM_BUILD_ROOT}%{_localstatedir}/log/%{name}
make DESTDIR=${RPM_BUILD_ROOT} install-strip
install -m 0644 scripts/vchanger.default ${RPM_BUILD_ROOT}%{_sysconfdir}/sysconfig/vchanger

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_libexecdir}/%{name}/*
%doc %{_docdir}/%{name}-%{version}/AUTHORS
%doc %{_docdir}/%{name}-%{version}/ChangeLog
%doc %{_docdir}/%{name}-%{version}/COPYING
%doc %{_docdir}/%{name}-%{version}/INSTALL
%doc %{_docdir}/%{name}-%{version}/NEWS
%doc %{_docdir}/%{name}-%{version}/README
%doc %{_docdir}/%{name}-%{version}/ReleaseNotes
%doc %{_docdir}/%{name}-%{version}/vchanger-example.conf
%doc %{_docdir}/%{name}-%{version}/example-vchanger-udev.rules
%doc %{_docdir}/%{name}-%{version}/vchangerHowto.html
%{_mandir}/man5/*
%{_mandir}/man8/*
%config(noreplace) %{_sysconfdir}/sysconfig/vchanger

%post
if [ $1 -eq 1 ] ; then
  /usr/bin/getent group tape &>/dev/null || /usr/sbin/groupadd -r tape
  /usr/bin/getent group bacula &>/dev/null || /usr/sbin/groupadd -r bacula
  /usr/bin/getent passwd bacula &>/dev/null || /usr/sbin/useradd -r -g bacula -d %{_localstatedir}/spool/bacula -s /bin/bash bacula
  if [ ! -d %{_localstatedir}/spool/vchanger ] ; then
    /bin/mkdir -p -m 0770 %{_localstatedir}/spool/vchanger
    /bin/chown bacula:tape %{_localstatedir}/spool/vchanger
  fi
  if [ ! -d %{_localstatedir}/log/vchanger ] ; then
    /bin/mkdir -p -m 0755 %{_localstatedir}/log/vchanger
    /bin/chown bacula:tape %{_localstatedir}/log/vchanger
  fi
  if [ ! -d %{_sysconfdir}/vchanger ] ; then
    /bin/mkdir -p -m 0755 %{_sysconfdir}/vchanger
  fi
fi

%changelog
* Fri Apr 3 2015 Josh Fisher <jfisher@jaybus.com>
- Initial spec file
