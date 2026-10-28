# Build diff16.exe for 16-bit MS-DOS with Open Watcom and libc16.
#   make                         fetch the pinned libc16 release into
#                                build/libc16, then build diff16.exe
#   make LIBC16_ROOT=../libc16   use a libc16 checkout built with its make
#   make host                    build diff16 for this host (tests)
#   make test                    host tests, then DOS tests in DOSBox-X
#
# diff16.exe is a compact-model program (-mc), compiled and linked against
# libc16's compact library only (its ABI is in libc16's docs/ABI.md), with
# the Open Watcom snapshot libc16 was built with: the libc16 tree's
# tools/fetch-watcom fetches it into build/tools.

LIBC16_ROOT ?= build/libc16
# A release tree, or a checkout, whose make stages the same layout in
# build/stage.
LIBC16_DIR  := $(if $(wildcard $(LIBC16_ROOT)/build/stage/VERSION),$(LIBC16_ROOT)/build/stage,$(LIBC16_ROOT))
LIBC16      := $(LIBC16_DIR)/lib/LIBC16C.LIB
TOOLS_ENV   := build/tools.env
WATCOM_ENV   = . ./$(TOOLS_ENV) && PATH=$$WATCOM/binl64:$$PATH

VERSION := $(shell cat VERSION)

CC     = wcc
CFLAGS = -q -zq -zl -zld -x -mc -0 -os -s -bt=dos -wx -we \
         -I$(LIBC16_DIR)/include '-DVERSION="$(VERSION)"'
LDOPTS = option quiet, nodefaultlibs, stack=4096

# Host compiler, for the tests.
HOSTCC     = cc
HOSTCFLAGS = -std=c89 -pedantic -Wall -Wextra -Werror -O2 \
             -D_POSIX_C_SOURCE=200809L '-DVERSION="$(VERSION)"'

NAMES   = main lines bigarr compare verify unified output temp error sys
SRCS    = $(NAMES:%=src/%.c)
HEADERS = src/d16.h

# Stress builds, for tests only: TINY searches at most 4 edits each way
# and keeps one page of each array in memory, so the search limit and
# the temporary files are used on small inputs; WEAK makes most line
# keys collide, so verify() has work to do. Their output must still
# patch correctly.
TINY_DEFS = -DCMP_DMAX=4L -DBA_RES_LIMIT=1
WEAK_DEFS = -DD16_WEAK_HASH

all: diff16.exe

# Objects go in build/r; the stress builds' in build/t and build/w.
R_OBJS = $(NAMES:%=build/r/%.obj)
T_OBJS = $(NAMES:%=build/t/%.obj)
W_OBJS = $(NAMES:%=build/w/%.obj)

diff16.exe: $(R_OBJS) $(LIBC16) $(TOOLS_ENV)
	$(WATCOM_ENV) && wlink $(LDOPTS), map=build/r/diff16.map \
	  format dos name $@ $(R_OBJS:%=file %) library $(LIBC16)
	sh scripts/check-map build/r/diff16.map $(LIBC16) build/r

build/dtiny.exe: $(T_OBJS) $(LIBC16) $(TOOLS_ENV)
	$(WATCOM_ENV) && wlink $(LDOPTS), map=build/t/dtiny.map \
	  format dos name $@ $(T_OBJS:%=file %) library $(LIBC16)

build/dweak.exe: $(W_OBJS) $(LIBC16) $(TOOLS_ENV)
	$(WATCOM_ENV) && wlink $(LDOPTS), map=build/w/dweak.map \
	  format dos name $@ $(W_OBJS:%=file %) library $(LIBC16)

build/r/%.obj: src/%.c $(HEADERS) VERSION $(LIBC16) $(TOOLS_ENV)
	mkdir -p build/r
	$(WATCOM_ENV) && $(CC) $(CFLAGS) -fo=$@ $<

build/t/%.obj: src/%.c $(HEADERS) VERSION $(LIBC16) $(TOOLS_ENV)
	mkdir -p build/t
	$(WATCOM_ENV) && $(CC) $(CFLAGS) $(TINY_DEFS) -fo=$@ $<

build/w/%.obj: src/%.c $(HEADERS) VERSION $(LIBC16) $(TOOLS_ENV)
	mkdir -p build/w
	$(WATCOM_ENV) && $(CC) $(CFLAGS) $(WEAK_DEFS) -fo=$@ $<

# The pinned libc16 release (scripts/fetch-libc16). A LIBC16_ROOT given
# on the command line must already be built.
ifeq ($(LIBC16_ROOT),build/libc16)
$(LIBC16): scripts/fetch-libc16
	scripts/fetch-libc16 $(LIBC16_ROOT)
endif

# The Open Watcom snapshot libc16 names, as shell assignments.
$(TOOLS_ENV): $(LIBC16)
	mkdir -p build
	watcom=$$(sh $(LIBC16_DIR)/tools/fetch-watcom $(CURDIR)/build/tools) \
	  && printf 'WATCOM=%s\nexport WATCOM\n' "$$watcom" > $@

# host builds
HOST = build/host/diff16 build/host/diff16-tiny build/host/diff16-weak

host: $(HOST)

build/host/diff16: $(SRCS) $(HEADERS) VERSION
	mkdir -p build/host
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $(SRCS)

build/host/diff16-tiny: $(SRCS) $(HEADERS) VERSION
	mkdir -p build/host
	$(HOSTCC) $(HOSTCFLAGS) $(TINY_DEFS) -o $@ $(SRCS)

build/host/diff16-weak: $(SRCS) $(HEADERS) VERSION
	mkdir -p build/host
	$(HOSTCC) $(HOSTCFLAGS) $(WEAK_DEFS) -o $@ $(SRCS)

test-host: $(HOST)
	sh tests/host.sh build/host

test-dos: diff16.exe build/dtiny.exe build/dweak.exe build/host/diff16
	sh tests/dos.sh $(CURDIR)/diff16.exe $(CURDIR)/build/dtiny.exe \
	  $(CURDIR)/build/dweak.exe $(CURDIR)/build/host/diff16

test: test-host test-dos

clean:
	rm -rf *.o *.obj *.err diff16.exe build/r build/t build/w build/host \
	  build/dtiny.exe build/dweak.exe

.PHONY: all host test test-host test-dos clean
