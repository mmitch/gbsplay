# $Id: Makefile,v 1.21 2003/08/27 15:07:02 ranma Exp $

include config.mk

prefix = /usr/local
exec_prefix = ${prefix}

bindir = ${exec_prefix}/bin
mandir = ${prefix}/man
man1dir = $(mandir)/man1

DESTDIR =

CFLAGS := -Wall -Wstrict-prototypes -Os -fomit-frame-pointer -fPIC $(EXTRA_CFLAGS)
LDFLAGS :=

SRCS := gbcpu.c gbhw.c gbsinfo.c gbsplay.c gbs.c $(EXTRA_SRCS)

.PHONY: all distclean clean install dist

# determine the object files

OBJS := $(patsubst %.c,%.o,$(filter %.c,$(SRCS)))

all: config.mk $(OBJS) gbsplay gbsinfo $(EXTRA_ALL)

# include the dependency files

%.d: %.c
	CC=$(CC) ./depend.sh $< $(CFLAGS) > $@

include $(OBJS:.o=.d)

distclean: clean
	find -regex ".*\.d" -exec rm -f "{}" \;
	rm -f ./config.mk ./config.err

clean:
	find -regex ".*\.\([aos]\|so\)" -exec rm -f "{}" \;
	find -name "*~" -exec rm -f "{}" \;
	rm -rf ./gbsplay

install: all install-default $(EXTRA_INSTALL)

install-default:
	install -d $(bindir)
	install -d $(man1dir)
	install -m 755 gbsplay $(bindir)/gbsplay
	install -m 644 gbsplay.1 $(man1dir)/gbsplay.1

install-gbsxmms.so:
	install -d $(DESTDIR)$(XMMS_INPUT_PLUGIN_DIR)
	install -m 644 gbsxmms.so $(DESTDIR)$(XMMS_INPUT_PLUGIN_DIR)/gbsxmms.so

uninstall: uninstall-default $(EXTRA_UNINSTALL)

uninstall-default:
	rm -f $(bindir)/gbsplay
	rm -f $(man1dir)/gbsplay.1
	-rmdir -p $(bindir)
	-rmdir -p $(man1dir)

uninstall-gbsxmms.so:
	rm -f $(DESTDIR)$(XMMS_INPUT_PLUGIN_DIR)/gbsxmms.so
	-rmdir -p $(DESTDIR)$(XMMS_INPUT_PLUGIN_DIR)

dist:	distclean
	install -d ./gbsplay
	install -m 755 configure ./gbsplay/
	install -m 755 depend.sh ./gbsplay/
	install -m 644 Makefile ./gbsplay/
	install -m 644 *.c ./gbsplay/
	install -m 644 *.h ./gbsplay/
	install -m 644 gbsplay.1 ./gbsplay/
	install -m 644 README ./gbsplay/
	tar -c gbsplay/ -vzf ../gbsplay.tar.gz
	rm -rf ./gbsplay

gbsinfo: gbsinfo.o gbs.o gbhw.o gbcpu.o
	$(CC) $(LDFLAGS) -o $@ gbsinfo.o gbhw.o gbcpu.o gbs.o -lz
gbsplay: gbsplay.o gbcpu.o gbhw.o gbs.o
	$(CC) $(LDFLAGS) -o $@ gbsplay.o gbcpu.o gbhw.o gbs.o -lm -lz

gbsxmms.so: gbcpu.o gbhw.o gbsxmms.o gbs.o
	$(CC) -shared $(LDFLAGS) -o $@ gbcpu.o gbhw.o gbs.o gbsxmms.o -lpthread -lz

.SUFFIXES: .i .s

.c.i:
	$(CC) -E $(CFLAGS) -o $@ $<
.c.s:
	$(CC) -S $(CFLAGS) -fverbose-asm -o $@ $<
