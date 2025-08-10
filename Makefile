#
# $ Makefile $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2023 Tomi Ollila
#	    All rights reserved
#
# Created: Tue 17 Jan 2023 23:58:46 EET too
# Last modified: Sun 10 Aug 2025 21:13:12 +0300 too

# one may be able to give command line input that breaks this,
# but if it is a footgun, it is their footgun...

# SPDX-License-Identifier: BSD 2-Clause "Simplified" License

.NOTPARALLEL:
SHELL = /bin/sh
.SHELLFLAGS = -eufc
.ONESHELL:
.SUFFIXES:
SUFFIXES =
MAKEFLAGS += --no-builtin-rules --no-builtin-variables
MAKEFLAGS += --warn-undefined-variables
unexport MAKEFLAGS

MCG1 := $(firstword $(MAKECMDGOALS))
MMCG :=

define haveargs =
ifeq ($(MCG1),$(strip $(1)))
match = 1
endif
MMCG += $(1)
.PHONY: $(1)
GOAL := $(1)
$(1): GOAL = $(strip $(1))
endef

define noargs =
MMCG += $(1)
.PHONY: $(1)
GOAL := $(1)
$(1): GOAL = $(strip $(1))
endef

# this help
$(eval $(call noargs, help ))
$(GOAL):
	@echo Commands:
#	#$(info Commands:) -- hah mix $(info) and other cmds misorder...
#	# gnu sed needed for 's/\n/   /...' part
	sed -n  -e 's/.*[e]val.*call.*args, *\(..[^_) ]*\) .*/  \1/; tp; h; d' \
		-e ':p; G; s/\n[ #]*/          /; s/\(.\{10\}\) */\1/p; ' \
		$(MAKEFILE_LIST)
	echo
	echo 'make mk rcopy test [I=I] may be convenient way to test w/ ui'
	echo ": rm -rf './mk_aarch64/' to remove development made files"
	echo ": rm -rf ./a[ar][rm][cv]* to remove rpmbuilt files"
	echo ": rm 'inetsshd', ldpreload-*.so, ... if such files created"
	echo


define shlead :=
set -- $(wordlist 2, 100, $(MAKECMDGOALS))
die () { printf '%s\n' '' "$$@" ''; exit 1; } >&2
x () { printf '+ %s\n' "$$*" >&2; "$$@"; }
x_exec () { printf '+ %s\n' "$$*" >&2; exec "$$@"; }
endef


.PHONY: _ssh_hostargs
ifdef D
_ssh_hostargs: ;
else
_ssh_hostargs:
	@$(info )
	$(info The Command '$(GOAL)' requires ssh access to the device.)
	$(info Set 'D=rhost' - either in environment or as a make variable.)
	$(info (Often just hostname is enough, but one may need more.))
	$(info Note: Multiple ssh commands may be run, some with -t included.)
	$(info FYI: make ssht D=, R=defaultuser@device may be useful ...)
	$(info )
	$(error )
endif


TD = /usr/share/test-md

# create /usr/share/test-md/{,qml,etc} over ssh (devel-su)
$(eval $(call noargs, rdirs ))
$(GOAL): _ssh_hostargs
	@$(shlead)
	x ssh -t $D "test -d $(TD) || { set -x;
	devel-su sh -c \"mkdir -p $(TD)/etc; chown \$$LOGNAME $(TD)\"; }"
	x_exec ssh $D "test -d $(TD)/qml ||
	{ mkdir $(TD)/qml; ln -s mad-developer.qml $(TD)/qml/test-md.qml; }"


# when I started I just had aarch64 -- armv7hl came later... (**)
mkd = mk_aarch64

# uh, first tune, to get "stamp" rules to work -- makes build rules
# more complicated -- and also may be reason for that .SECONDARY: below
# (i care later, if ever -- now hardcoding rerequisites for build rule)
SF = mad-developer.qml mad-developer.py InfoPage.qml
SF += inetsshd ldpreload-sshdivert.so exitsshconns
SF += test-sshd.sh # gets copied to .../qml/ -- which is tolerable
ST = $(SF:%=$(mkd)/.%.up)
#$(error $(ST))

$(mkd):
	mkdir $@

$(mkd)/.%.up: $(mkd)/%
	scp $< $D:$(TD)/
	touch $@

# order of the above and below matters, to not copy e.g. ./inetssh (if exists)

$(mkd)/.%.up: % | $(mkd)
	sed s:/mad-developer:/test-md: $< \
	| ssh $D 'cat >$(TD)/qml/$<'
	touch $@


.SECONDARY: # this is to keep *all* "intermediate" files...

# (**) ...therefore (for now) only aarch dev here...
cimage = sailfishos-platform-sdk-aarch64:latest
tcdefs = -DAPP_SHARE_DIR=\"/usr/share/test-md\" -DPORT=2222

$(mkd)/%: %.c
	./run-in-podman...sh $(cimage) sh $< $@ $(tcdefs)

$(mkd)/%.so: %.c
	./run-in-podman...sh $(cimage) sh $< $@ $(tcdefs)


# copy material over ssh, to /usr/share/test-md/{,qml}
$(eval $(call noargs, rcopy ))
$(GOAL): _ssh_hostargs $(ST)
	@echo Do ln -s mad-developer.qml .../qml/test-md.qml manually.
	echo Also, scp ssh_host_ed25519_key and then move it in place manually.


# (just) do the development test build...
$(eval $(call noargs, mk ))
$(GOAL): mad-developer.qml mad-developer.py
$(GOAL): | $(mkd)
$(GOAL): $(mkd)/inetsshd $(mkd)/ldpreload-sshdivert.so $(mkd)/exitsshconns
#	#@: this line is not needed, but if there mkdir $(mkd) not done :O


# devel-su rm -rf /usr/share/test-md/ over ssh connection
$(eval $(call noargs, rrm ))
$(GOAL): _ssh_hostargs
	ssh $D 'set -x; devel-su rm -rf /usr/share/test-md'


# test run on device: append I={anything} to run via invoker
$(eval $(call noargs, try ))
$(GOAL): _ssh_hostargs
ifdef I
	ssh -t $D $(INVOPTS) sailfish-qml test-md
else
	ssh -t $D sailfish-qml test-md
endif
INVOPTS = invoker -vv --single-instance --type=silica-qt5

# make 1-day persistent ssh tunnel...
$(eval $(call noargs, ssht ))
$(GOAL):
ifndef D
	$(error give D='ssh dest' as make option (e.g. ','))
endif
ifndef R
	$(error give R='[user@]host [other opts]' as make option)
endif
	$(info Checking/creating persistent connection for '$D')
	@set -x; z=`ssh -O check "$D" 2>&1` && \
	{ : ssh $D -O exit to exit if so desired; exit; \
	} || case $$z in 'Control socket connect'*) ;; *) \
		printf '%s\n(in ~/.ssh/config)\n' "$${z%?}"; \
		exit 1; \
	     esac; \
	z=$${z%)*}; z=$${z#*\(}; \
	test -e "$$z" && rm "$$z"; \
	ssh -oControlPath=$$z -M -oControlPersist=1d "$R" date; date


# rpmbuild using podman / sailfishos-platform-sdk-aarch64 (or -armv7hl)
$(eval $(call haveargs, rpmb ))
$(GOAL):
	@$(shlead)
	test $$# = 1 || die "Usage: $(MAKE) $@ '(32|64)'"
	case $$1 in 32) arch=armv7hl ;; 64) arch=aarch64
		;; *) due "'$$1' not '32' nor '64'"
	esac
	test -f mad-developer.spec
	test -d build-rpms || mkdir build-rpms
	script -ec 'set -x
	build_name_fmt="%%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm"
	exec \
	./run-in-podman...sh sailfishos-platform-sdk-'"$$arch"':latest \
		rpmbuild --build-in-place -D "%_rpmdir $$PWD/build-rpms" \
			-D "%_buildhost buildhost" \
			-D "%_build_name_fmt $$build_name_fmt" \
			--target='"$$arch"' -bb mad-developer.spec
	' build-rpms/typescript-rpmbuild-$$arch
	x_exec ls -goF build-rpms

$(eval $(call haveargs, rpmbuild)) # hidden from help, spece before ) missing
$(GOAL): rpmb


# view image file (scaled) using feh(1)
$(eval $(call haveargs, feh ))
$(GOAL):
	@$(shlead)
	test $$# -ge 1 || die "Usage: $(MAKE) $@ file [zoom] [bgcolor]"
	fp=$$1
	shift
	test -f "$$fp" || die "'$$fp': no such file"
	case $${1-} in [1-9]|1[0-9]) z=$$1; shift ;; *) z=8 ;; esac
	test $$# -ge 1 && _B_color="-B $$1" || _B_color=
#	# --zoom unusable (window geometry does not follow) -- therefore
	set_wx_wy () {
		set -- `feh -ls "$$fp"`
		test "$$3.$$4" = WIDTH.HEIGHT ||
		die 'feh -ls output' "$$*" 'not in expected format'
		shift 8
		wx=$$3 wy=$$4
	}; set_wx_wy
	wx=$$((wx * z)) wy=$$((wy * z))
	x_exec feh --title='%wx%h %z %f' $$_B_color --force-aliasing \
		-s -g $$wx'x'$$wy -Z $$fp


match ?=
REST := $(filter-out $(MMCG), $(MAKECMDGOALS))
#$(info $(REST) / $(match))
.PHONY: $(REST)
$(REST):
ifndef match
	@echo "make: +++ No rule to make target '$@'. stop"
	false
else
	@:
endif


#$(foreach v, $(.VARIABLES), $(info $(origin $v) $(flavor $v) $v = $($v)))

# Local variables:
# mode: makefile
# End:
