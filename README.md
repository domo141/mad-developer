
Mad Developer (for SFOS)
========================

This is Mad Developer version for Sailfish OS,
partially similar as the one done for N900 in 2009.

The display of Network configuration is somewhat
the same as in N900.

Rest of the features of the N900 version are implemented in
Sailfish OS already so those did not need to be implemented.

--

The prominent feature of this Mad Developer version is starting
sshd in a way that both `nemo` and `defaultuser` can be used
as usernames to log in -- the logged-in username will be the one
configured in the device. More usernames for the same puppose
can be added to `/usr/share/mad-developer/loginnames`, which
can make the caller's ssh command line (or configuration)
even easier...

Also, when sshd is started from this application, and user logs
in via `rndis0` device using address `192.168.2.15`, the ED25519
host key will be same on all devices; the fingerprint of the key is:

    SHA256:qqqqqI3jMdIvxbHyO6DY/iF0c5KZunrbwp7v405vW4Q

When swapping devices the `StrictHostKeyChecking` check does not
need to be disabled for usb connections when all devices start
sshd via this application.

--

No system files has been altered while installing/running this
application. The `LD_PRELOAD` hac^H^H^H^H feature of the dynamic
linker is used to "divert" respective library and system calls
to make the required magic happen.

--

Finally, there is an option to exit all inbound ssh connections;
exiting ssh daemon does not disconnect the currently live ssh
tcp sessions (which is good thing). This option does not exit
any other connections, nor exit running sshd if there.
