
# I build this the following way (as I don't know any other way...)

# ./run-in-podman...sh sailfishos-platform-sdk-aarch64:latest \
#      rpmbuild --build-in-place -D "%""_rpmdir $PWD" \
#               --target=aarch64 -bb mad-developer.spec
# extra ""s above is for rpmbuild not to nitpick -- can be left out...
# -- replace (the 2) 'aarch64's w/ 'armv7hl's above for 32-bit build

%define _wd %_rpmdir/wd1-%_target_platform

Name:        mad-developer
Summary:     Mad Developer
Version:     2023
Release:     3
License:     BSD-2-Clause
Requires:    sailfishsilica-qt5 >= 0.10.9
Requires:    pyotherside-qml-plugin-python3-qt5
Requires:    libsailfishapp-launcher
BuildRequires:  desktop-file-utils


%description
Mad Developer 2023

Show device IPv4 addresses, LD_PRELOADed sshd execution.

- nemo & defaultuser both work when logged in via ssh
- host key is the same on all devices when login via
  usb (rndis0) interface using address 192.168.2.15

%prep
sed '$q; s/^/: /' "$0"
#env | sort
#set -x; : : "$RPM_ARCH" : $wd : :; gcc -v
gcc -v 2>&1 | grep -wqE %_target_cpu-meego-linux-gnu'(eabi)?' || { exec >&2
  echo "gcc does not compile for %_target_cpu-meego-linux-gnu[eabi]"; exit 1; }
#id
#%setup -q -n %{name}-%{version}
#%setup -q
#cp $OLDPWD/%{name}* .


%build

test -d %_wd || mkdir %_wd
cd %_wd
#strace -ff -oxx sh ../ldpreload-sshdivert.c
sh ../../ldpreload-sshdivert.c
sh ../../inetsshd.c
sh ../../exitsshconns.c
strip ldpreload-sshdivert.so inetsshd exitsshconns


%install
set -euf
# protection around "user error" with --buildroot=$PWD (tried before
# --build-in-place) -- rpmbuild w/o sudo and container image default
# uid/gid saved me from deleting all files from current directory).
# "inverse logic" even though it is highly unlikely rm -rf fails...
test ! -f %{buildroot}/%{name}.qml && rm -rf %{buildroot} ||
rm -rf %{buildroot}%{_datadir}
umask 022
mkdir -p %{buildroot}%{_datadir}/
mdd=%{buildroot}%{_datadir}/%{name}
mkdir $mdd/ $mdd/qml/ $mdd/etc/ \
      %{buildroot}%{_datadir}/applications/ \
      %{buildroot}%{_datadir}/icons/ \
      %{buildroot}%{_datadir}/icons/hicolor/ \
      %{buildroot}%{_datadir}/icons/hicolor/86x86/ \
      %{buildroot}%{_datadir}/icons/hicolor/86x86/apps/
install -m 644 %{name}.qml InfoPage.qml %{name}.py $mdd/qml/.
install -m 755 %_wd/inetsshd %_wd/exitsshconns $mdd/.
install -m 644 %_wd/ldpreload-sshdivert.so $mdd/.
#nstall -m 755 %{name}.sh $mdd/.
install -m 444 ssh_host_ed25519_key.pub $mdd/etc/.
install -m 400 ssh_host_ed25519_key $mdd/etc/.
install -m 644 %{name}.png %{buildroot}%{_datadir}/icons/hicolor/86x86/apps/.
install -m 644 %{name}.desktop %{buildroot}%{_datadir}/applications/.

set +f
desktop-file-install --delete-original \
      --dir %{buildroot}%{_datadir}/applications \
      %{buildroot}%{_datadir}/applications/*.desktop


%clean
set -euf
# protection around "user error" with --buildroot=$PWD ...
test -f %{buildroot}/%{name}.qml && rm -rf %{buildroot}%{_datadir} ||
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/86x86/apps/%{name}.png
