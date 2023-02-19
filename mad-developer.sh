#!/bin/sh
#
# $ mad-developer.sh $ -- work in progress, therefore not much promoted...
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2023 Tomi Ollila
#	    All rights reserved
#
# Created: Thu 26 Jan 2023 21:12:03 EET too
# Last modified: Fri 27 Jan 2023 17:55:32 +0200 too

case ${BASH_VERSION-} in *.*) set -o posix; shopt -s xpg_echo; esac
case ${ZSH_VERSION-} in *.*) emulate ksh; esac

set -euf  # hint: (z|ba|da|'')sh -x thisfile [args] to trace execution

PATH='/sbin:/usr/sbin:/bin:/usr/bin'; export PATH

die () { printf '%s\n' '' "$@" ''; exit 1; } >&2

x () { printf '+ %s\n' "$*" >&2; "$@"; }
x_eval () { printf '+ %s\n' "$*" >&2; eval "$*"; }
x_eval_exec () { printf '+ %s\n' "$*" >&2; eval "exec $*"; }
x_exec () { printf '+ %s\n' "$*" >&2; exec "$@"; }

if test "${EUID:=`/usr/bin/id -u`}" = 0
then
	r=${1-} X=×
else
	r=× X='$r'
fi

test $# -gt 0 || { exec >&2; echo
	echo "Usage: ${0##*/} command [args]"
	echo
	echo Commands:
	sed -n '/EUID/d; /'"$X"'/d; s/# / /; s/^if test.* = /  /p' "$0"
	echo
	echo 'Use e.g. `devel-su` to gain root, for more commands'
	echo "-- after you've seen the code and trust it."
	echo
	exit
}

if test "$1" = pgrep    # pgrep mad-developer processes
then
	x_exec pgrep -f -a mad-developer
	exit not reached
fi

if test "$r" = pkill    # pkill mad-developer processes
then
	x pgrep -f -a mad-developer
	x_exec pkill -f mad-developer
	exit not reached
fi

mddir=/usr/share/mad-developer

if test "$1" = usercat  # list users in "loginnames"
then
	x_exec cat $mddir/loginnames
	exit not reached
fi

if test "$r" = useradd  # add user to "loginnames"
then
	#case $2 in *["$IFS"]*) die "Whitespace in $1" ;; esac
	case ${2-} in '') die "$1: need login name to add"
		;; *[!0-9a-z]*) die "Unsupported chars in $2"
	esac
	x_eval "echo $2 >> $mddir/loginnames"
	x_exec cat $mddir/loginnames
	exit not reached
fi

if test "$r" = userdel  # remove user from "loginnames"
then
	case ${2-} in '') die "$1: need login name to remove"
		;; *[!0-9a-z]*) die "Unsupported chars in $2"
	esac
	x sed -i "/^$2$/d" $mddir/loginnames
	x_exec cat $mddir/loginnames
	exit not reached
fi

if test "$1" = ls       # ls -lRF `rpm -qi mad-developer` | sed ...
then
	echo
	x_eval 'ls -gdF `rpm -ql mad-developer` | sed "s/  //; s/ ... .. ..:..//; s/       //"'
	echo
	exit
fi

die "'$1': unknown command"

# Local variables:
# mode: shell-script
# sh-basic-offset: 8
# tab-width: 8
# End:
# vi: set sw=8 ts=8
