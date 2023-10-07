#!/bin/sh
#
# $ sl -- secure login persistence ... $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
# Created: Sat 20 May 2023 18:30:23 EEST too
# Last modified: Sat 02 Sep 2023 10:27:11 +0300 too

# SPDX-License-Identifier: Unlicense

case ${BASH_VERSION-} in *.*) set -o posix; shopt -s xpg_echo; esac
case ${ZSH_VERSION-} in *.*) emulate ksh; esac

set -euf  # hint: (z|ba|da|'')sh -x thisfile [args] to trace execution

die () { printf '%s\n' '' "$@" ''; exit 1; } >&2

x () { printf '+ %s\n' "$*" >&2; "$@"; }
x_exec () { printf '+ %s\n' "$*" >&2; exec "$@"; die "exec '$*' failed"; }

test $# -gt 0 ||
die "Usage: ${0##*/} {name} [=[user@]{host}] ({time} [sshopts] [rcmd [args]]...|{cmd})" \
    '' \
    "E.g.  ${0##*/} , =192.168.2.15 4h date; date" \
    "Then: ${0##*/} , date; ssh , date" '' \
    'Commands (if seen as 2nd arg, after {name}):' \
    '   exit: reguest master connection to exit' \
    '   sfs:  sshfs remote to $HOME/mnt/...' \
    '   umnt: fusermount -u $HOME/mnt/...'


case $1 in */*) die "Slashes (/) in '$1'"
	;; *:*) test "${2-}" = sfs || die "Colon(s) in '$1'"
	;; *["$IFS"]*) die "Whitespace in '$1'"
esac

if test $# -gt 1
then
 if test "$2" = exit
 then	x_exec ssh -O exit "$1"
	exit not reached
 fi

 if test "$2" = sfs # add path...
 then
	case $1 in *:*) h=${1%%:*} p=${1#*:} ;; *) h=$1 p= ;; esac
	if test $# -gt 2
	then shift 2; x_exec sshfs "$h:$p" "$@"; exit not reached
	fi
	test . -ef "$HOME" || x cd "$HOME"
	case $h in .* | ,.* | ,*,.* ) d=,$h ;; *) d=$h ;; esac
	test -d mnt/$d || x mkdir -p mnt/$d
	x_exec sshfs "$h:$p" mnt/"$d"
	exit not reached
 fi
 if test "$2" = umnt # add path... (?)
 then
	case $1 in *:*) h=${1%%:*} p=${1#*:} ;; *) h=$1 p= ;; esac
	test . -ef "$HOME" || x cd "$HOME"
	case $h in .* | ,.* | ,*,.* ) d=,$h ;; *) d=$h ;; esac
	x_exec fusermount -u mnt/"$d"
	exit not reached
 fi
fi

# else create ssh tunnel #

echo "Checking/creating persistent connection for '$1'" >&2
z=`ssh -O check "$1" 2>&1` &&
	{ printf '%s\n' "${z%?} (ssh $1 -O exit to exit)" >&2; z=1
	} || case $z in 'No ControlPath specified'*)
		printf '%s (in ~/.ssh/config)\n' "${z%?}" >&2
		exit 1
	     esac

cl=$*

h=${1-}; shift

case ${1-} in =*) r=${1#?}; shift ;; *) r=$h ;; esac

timefmt () {
	case $1 in h|d|w) return 0
		;; *[!0-9]*?) return 1
		;; [1-9]*[smhdw]) return 0
	esac
	return 1
}

timefmt "${1-}" && { time=$1; shift; } || time=

if test "$z" = 1 # connection up (already)
then
	x_exec ssh "$h" "$@"
	exit not reached
fi

usage () {
	bn0=${0##*/}
	rest='{time}(s|m|h|d|w) [command [args]]'
	die "$@" '' "Usage: $bn0 {name} [=[{user}@]{host}] $rest" \
	    '' ": E.g.; $bn0 , ={user}@{host} 4h date; date" \
	    '' ': Then; ssh , date; date' '' \
	    "Note: [user@]host can be added to .ssh/config, also for ','" \
	    '      the above is an example for when not there...'
}

batch=false

test "$time" || {
 if ! $batch && test -t 0
 then
	read -p "No live connection for '$h': Enter {time}(s|m|h|d|w) " time
	timefmt "$time" || die "time '$time' not in format [1-9]...(s|m|h|d|w)"
 else
	usage "No live connection for '$h', so {time}(s|m|h|d|w) needed"
 fi
}

z=${z%)*}; z=${z#*\(}
test -e "$z" && rm "$z"
case $time in h|d|w) time=1$time; esac
x_exec ssh -oControlPath=$z -M -oControlPersist=$time "$r" "$@"


# Local variables:
# mode: shell-script
# sh-basic-offset: 8
# tab-width: 8
# End:
# vi: set sw=8 ts=8
