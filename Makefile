# $Id: Makefile,v 1.22 2003/08/28 14:37:03 ranma Exp $

include config.mk

prefix      := /usr/local
exec_prefix := $(prefix)

bindir      := $(exec_prefix)/bin
mandir      := $(prefix)/man
man1dir     := $(mandir)/man1

DESTDIR :=

CFLAGS  := -Wall -Wstrict-prototypes -Os -fomit-frame-pointer -fPIC
LDFLAGS :=

CFLAGS  += $(EXTRA_CFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)

export CC CFLAGS LDFLAGS

objs_gbslib  := gbcpu.o gbhw.o gbs.o
objs_gbsplay := gbsplay.o
objs_gbsinfo := gbsinfo.o
objs_gbsxmms := gbsxmms.o

objs := $(objs_gbslib) $(objs_gbsplay) $(objs_gbsinfo)
dsts := gbslib.a gbsplay gbsinfo

ifeq ($(build_xmmsplugin),y)
objs += $(objs_gbsxmms)
dsts += gbsxmms.so
endif

.PHONY: all distclean clean install dist

all: config.mk $(objs) $(dsts) $(EXTRA_ALL)

# include the dependency files

include $(patsubst %.o,%.d,$(filter %.o,$(objs)))

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

gbslib.a: $(objs_gbslib)
	$(AR) r $@ $?
gbsinfo: $(objs_gbsinfo) gbslib.a
	$(CC) $(LDFLAGS) -o $@ $? -lz
gbsplay: $(objs_gbsplay) gbslib.a
	$(CC) $(LDFLAGS) -o $@ $? -lz -lm

gbsxmms.so: $(objs_gbsxmms) gbslib.a
	$(CC) -shared $(LDFLAGS) -o $@ $? -lpthread -lz

# rules for suffixes

.SUFFIXES: .i .s

.c.o:
	@echo CC $<
	@$(CC) $(CFLAGS) -c -o $@ $<
.c.i:
	$(CC) -E $(CFLAGS) -o $@ $<
.c.s:
	$(CC) -S $(CFLAGS) -fverbose-asm -o $@ $<

# rules for generated files

config.mk: configure
	./configure
%.d: %.c config.mk
	@echo DEP $<
	@./depend.sh $< $(CFLAGS) > $@
