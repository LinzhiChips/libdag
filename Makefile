#
# Copyright (C) 2021 Linzhi Ltd.
#
# This work is licensed under the terms of the MIT License.
# A copy of the license can be found in the file COPYING.txt
#

MKTGT = $(MAKE) -f Makefile.core

.PHONY:		all host arm clean spotless

all:		host arm

host:
		$(MKTGT) OBJDIR=./ $(SUB_TARGET)

arm:
		$(MKTGT) OBJDIR=arm/ CROSS=arm-linux- $(SUB_TARGET)

clean:
		$(MKTGT) OBJDIR=./ clean
		$(MKTGT) OBJDIR=arm/ clean

spotless:
		$(MKTGT) OBJDIR=./ spotless
		$(MKTGT) OBJDIR=arm/ spotless

# ----- Install/uninstall -----------------------------------------------------

.PHONY:		install install-host install-arm
.PHONY:		uninstall uninstall-host uninstall-arm

PREFIX ?= /usr/local
INSTALL ?= install

INSTALL_INCLUDES = common.h dag.h keccak.h blake2.h mine.h dagio.h dagalgo.h

install:        install-host install-arm

install-common:
		for n in $(INSTALL_INCLUDES); do \
		    $(INSTALL) -D -m 0644 -t $(PREFIX)/include/linzhi $$n || \
		    exit; \
		done

install-host:	install-common
		$(INSTALL) -D -m 0644 -t $(PREFIX)/lib/ libdag.a

install-arm:	install-common
		$(INSTALL) -D -m 0644 arm/libdag.a $(PREFIX)/lib/libdag-arm.a

uninstall:	uninstall-host uninstall-arm

uninstall-common:
		for n in $(INSTALL_INCLUDES); do \
		    rm -f $(PREFIX)/include/linzhi/$$n; done

uninstall-host:
		rm -f $(PREFIX)/include/linzhi/libdag.a

uninstall-arm:
		rm -f $(PREFIX)/include/linzhi/libdag-arm.a

# ----- Sharing ---------------------------------------------------------------

.PHONY:         share

DEST = $(shell git rev-parse --show-toplevel)/../libdag
MK_C = $(shell git rev-parse --show-toplevel)/common/Makefile.c-common

share:		Makefile Makefile.target Makefile.core \
		common.h dag.c dag.h keccak.c keccak.h blake2b-ref.c blake2.h mine.h mine.c \
		target.c mdag.h mdag.c util.h util.c dagio.h dagio.c \
		dagalgo.c dagalgo.h
		cp $^ $(DEST)/
		cp $(MK_C) $(DEST)/

