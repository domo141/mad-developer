#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# $ mad-developer.py $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2022 Tomi Ollila
#	    All rights reserved
#
# Created: Mon 06 Jun 2022 23:28:40 EEST too (device-ip)
# Next created: Sat 14 Jan 2023 16:57:01 +0200 too
# Last modified: Sat 18 Feb 2023 01:19:51 +0200 too

# SPDX-License-Identifier: BSD 2-Clause "Simplified" License

#pylint: disable=W0107,C0103,C0114,C0115,C0116,C0321,R0903

from datetime import datetime
from socket import socket
from fcntl import ioctl
from os import pipe, close, fork, _exit, dup2, execl, waitpid, read, write
from threading import Thread

if __name__ != '__main__':
    import pyotherside #pylint: disable=E0401
    pass

# ( cd /usr/include && grep -r SIOCGIF )
SIOCGIFADDR = 0x8915
SIOCGIFNETMASK = 0x891B

mm = { 255:8, 127:7, 63:6, 31:5, 15:4, 7:3, 3:2, 1:1, 0:0 }

def _ip_pl(sd, name):
    try:
        i = name.ljust(256, b'\0')
        a = ioctl(sd, SIOCGIFADDR, i)[20:24]
        ip = f'{a[0]}.{a[1]}.{a[2]}.{a[3]}'
        a = ioctl(sd, SIOCGIFNETMASK, i)[20:24]
        pl = f'/{mm[a[0]]+mm[a[1]]+mm[a[2]]+mm[a[3]]}'
    except OSError:
        return '-', '-'
    return ip, pl


def usb_wlan_ipv4s():
    s = socket() # defaults to AF_INET, SOCK_STREAM|SOCK_CLOEXEC, IPPROTO_IP
    sd = s.fileno()
    usb_ip, usb_pl = _ip_pl(sd, b'rndis0')
    wlan_ip, wlan_pl = _ip_pl(sd, b'wlan0')

    return (usb_ip, usb_pl, wlan_ip, wlan_pl,
            f'updated: {datetime.now().strftime("%H:%M:%S")}')


def start_inetsshd(current):
    if current:
        if thread_ctx.w is not None: close(thread_ctx.w); thread_ctx.w = None
        if thread_ctx.thread is not None: thread_ctx.thread.join()
        return 0, 'inetsshd exited'
    # else
    r, wc = pipe() # fyi: strace: pipe2([3, 4], O_CLOEXEC) = 0
    rc, w = pipe()
    #pyotherside.send('pkexec inetsshd', 0, msg) # "blocks" if done here...
    pid = fork()
    if pid == 0:
        # child
        dup2(rc, 0)
        dup2(wc, 2)
        # note: CLOEXEC fd's will be closed
        #execl('/usr/share/mad-developer/inetsshd', 'inetsshd')
        execl('/usr/bin/pkexec', 'pkexec', '--disable-internal-agent',
              '/usr/share/mad-developer/inetsshd')
        write(2, b'execl failed...')
        _exit(0)
    # parent
    close(rc)
    close(wc)
    msg = read(r, 512).strip()
    if msg.find(b'inetsshd listening') < 0:
        # here if inetsshd failed early
        close(r)
        close(w)
        waitpid(pid, 0)
        return 0, msg if msg != b'' else 'tapahtui virhe ;/' # to b or not to b
    # OK
    thread_ctx.pid = pid
    thread_ctx.thread = Thread(target=sshd_thread, args=(r,w), daemon=True)
    thread_ctx.thread.start()
    return 1, msg


class thread_ctx:
    thread = None
    pid = None
    w = None
    pass


def sshd_thread(r, w):
    thread_ctx.w = w
    try:
        while True:
            msg = read(r, 512).strip()
            if msg == b'': break
            pyotherside.send('msg', 1, msg)
            pass
        pass
    except OSError: pass # OSError: [Errno 9] Bad file descriptor
    finally:
        close(r)
        waitpid(thread_ctx.pid, 0)
        thread_ctx.pid = None
        if thread_ctx.w is not None:
            close(w)
            thread_ctx.w = None
            pyotherside.send('msg', 0, 'inetsshd exited')
            pass
        thread_ctx.thread = None
        pass
    pass


def run_exitsshconns():
    r, wc = pipe() # fyi: strace: pipe2([3, 4], O_CLOEXEC) = 0
    pid = fork()
    if pid == 0:
        # child
        dup2(wc, 2)
        # note: CLOEXEC fd's will be closed
        execl('/usr/bin/pkexec', 'pkexec', '--disable-internal-agent',
              '/usr/share/mad-developer/exitsshconns')
        write(2, b'execl failed...')
        _exit(0)
    # parent
    close(wc)
    waitpid(pid, 0)
    msg = read(r, 512).strip()
    close(r)
    return msg


def main():
    print(usb_wlan_ipv4s())
    pass


if __name__ == '__main__':
    main()
    pass  # pylint: disable=W0107
