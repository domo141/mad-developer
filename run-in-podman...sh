#!/bin/sh
#
# $ run-in-podman...sh $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2023 Tomi Ollila
#	    All rights reserved
#
# Created: Sat 14 Jan 2023 15:37:04 EET too
# Last modified: Sun 19 Feb 2023 12:06:11 +0200 too

# (Ø) public domain, like https://creativecommons.org/publicdomain/zero/1.0/

# When using coderus/sailfishos-platform-sdk-aarch64 docker images
# (tried images dated 2021-06-08 and 2022-08-14) the aarch64 (and armv7hl)
# cross compilers are located at /opt/cross/bin/. Also, when
# /opt/cross/bin/aarch64-meego-linux-gnu-gcc tries to run as(1), it just
# runs 'as' (i.e. not /opt/cross/bin/aarch64-meego-linux-gnu-as).
# (and then some other things that came up in the way to get c code compiled)
#
# The other thing that is to be done, when running this particular container
# image using (rootless) podman is to run with `-u root` -- so that uidmap
# from container root (0) maps to the regular user uid outside the running
# container (permission denied errors with uid=100000(mersdk)).

case ${BASH_VERSION-} in *.*) set -o posix; shopt -s xpg_echo; esac
case ${ZSH_VERSION-} in *.*) emulate ksh; esac

set -euf  # hint: (z|ba|da|'')sh -x thisfile [args] to trace execution

saved_IFS=$IFS; readonly saved_IFS

die () { printf '%s\n' '' "$@" ''; exit 1; } >&2

x () { printf '+ %s\n' "$*" >&2; "$@"; }
x_exec () { printf '+ %s\n' "$*" >&2; exec "$@"; die "exec '$*' failed"; }

if test $# = 0 # checked --in-container-- too -- just that $# != 0 there always
then
	echo
	echo Enter a container image listed below as an argument
	echo
	cibp='sailfishos-platform-sdk-*'
	cib6='sailfishos-platform-sdk-aarch64'
	cib3='sailfishos-platform-sdk-armv7hl'
	podman images \
		--format='  {{.Repository}}:{{.Tag}}   {{.CreatedSince}}' $cibp
	echo
	echo "( if ^list^ empty, podman pull coderus/$cib6:latest"
	echo "                or podman pull coderus/$cib3:latest )"
	echo
	exit
fi

if test "$1" != --in-container--  # this block run on host #
then
	test $# -gt 1 || die "Usage: $0 {container-image} command [args]"

	h=${1##*/}
	case $h in *[!a-z0-9:.-]*) die "Unsupported chars in '$h'"
		;; sailfishos-platform-sdk-aarch64:*) arch=aarch64
		;; sailfishos-platform-sdk-armv7hl:*) arch=armv7hl
		;; *) die "'$1': unknown container image"
	esac
	ci=$1
	shift 1
	h=${h%:*}-${h#*:} # {name}:{tag} -> {name}-{tag}
	case $h in *:*) h=${h%%:*}; esac # just ensure no :s left

	x_exec podman run --pull=never --rm -it --privileged \
		-v "$PWD:$PWD" -w "$PWD" -h $h -u root "$ci" \
		/bin/bash "$0" --in-container-- $arch "$@"
	exit not reached
fi

# rest of this file is executed in container using /bin/bash

#set -x

case $2 in *aarch64*) bp=aarch64-meego-linux-gnu
	;; *armv7hl*) bp=armv7hl-meego-linux-gnueabi
	;; *) die "internal error: '$2' not 'aarch64' nor 'armv7hl'"
esac

set +f
for ldd in /srv/mer/targets/SailfishOS-*-$2 ; do :; done # last one
test -d "$ldd" || die "Cannot find a dir based on '$ldd'"
set -f

cb='gcc as ld strip'
for f in $cb
do test -f /opt/cross/bin/$bp-$f ||
	die "/opt/cross/bin/$bp-$f does not exist: use other container image"
done
# hmm /srv/mer/toolings/SailfishOS-*/ is there too -- found later than /opt/...

# with path as
#   /usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
# somehow ld is not found, strace told it is trying to search many
# places (and names), but not /opt/cross/bin/aarch64-meego-linux-gnu-ld)
# so link tools to /usr/local/bin and add /opt/cross/bin to the path...
# .. ok, '+ /usr/lib/rpm/brp-strip /usr/bin/strip' in final rpmbuild stage
# fails (i486 strip). so. overwriting binaries in /usr/bin)
# ...got a bit lost w/ all tries. finally need that /opt/cross/bin in path...

for f in $cb
#do	ln -s /opt/cross/bin/$bp-$f /usr/local/bin/${f##*-}
do	ln -sf /opt/cross/bin/$bp-$f /usr/bin/${f##*-}
done
#ls -l /usr/local/bin/${f##*-}

# gcc -v gave more hints what to do
ln -s $ldd /opt/cross/$bp/sys-root

# add a bit more information to hostname if it ends with '-latest'...
hostname=`hostname`
#read hostname < /etc/hostname || : # fails, when there is no trailing '\n'
case $hostname in *-latest)
	while read line
	do case $line in VERSION_ID=*)
		dots_to_dashed () {
			IFS=.; set -- $1; IFS=-; dashed=$*; IFS=$saved_IFS;
		}
		dots_to_dashed ${line#*=}
		hostname=${hostname%-latest}-$dashed
		hostname $hostname
		#echo $hostname > /etc/hostname
		break
	esac; done < /etc/os-release
esac


shift 2
PATH=/opt/cross/bin:$PATH x_exec "$@"
#x_exec "$@"


# Local variables:
# mode: shell-script
# sh-basic-offset: 8
# tab-width: 8
# End:
# vi: set sw=8 ts=8
