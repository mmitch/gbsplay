# $Id: Makefile,v 1.52 2003/11/30 15:03:49 ranma Exp $

noincludes  := $(patsubst clean,yes,$(patsubst distclean,yes,$(MAKECMDGOALS)))

ifneq ($(noincludes),yes)
-include config.mk
endif

prefix      := /usr/local
exec_prefix := $(prefix)

bindir      := $(exec_prefix)/bin
mandir      := $(prefix)/man
man1dir     := $(mandir)/man1
man5dir     := $(mandir)/man5
docdir      := $(prefix)/share/doc/gbsplay

DESTDIR :=
DISTDIR := gbsplay-$(VERSION)

CFLAGS  := -Wall -std=gnu99 -Wstrict-prototypes -g -Os
LDFLAGS :=

CFLAGS  += $(EXTRA_CFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)

export CC CFLAGS LDFLAGS

docs           := README TODO HISTORY gbsplayrc_sample

mans           := gbsplay.1    gbsinfo.1    gbsplayrc.5
mans_src       := gbsplay.in.1 gbsinfo.in.1 gbsplayrc.in.5

objs_libgbspic := gbcpu.po gbhw.po gbs.po cfgparser.po crc32.po
objs_libgbs    := gbcpu.o  gbhw.o  gbs.o  cfgparser.o  crc32.o
objs_gbsplay   := gbsplay.o util.o
objs_gbsinfo   := gbsinfo.o
objs_gbsxmms   := gbsxmms.o

objs := $(objs_libgbs) $(objs_gbsplay) $(objs_gbsinfo)
dsts := libgbs.a libgbspic.a gbsplay gbsinfo

ifeq ($(build_xmmsplugin),y)
objs += $(objs_gbsxmms)
dsts += gbsxmms.so
endif

.PHONY: all distclean clean install dist libgbs

all: config.mk $(objs) $(dsts) $(mans) $(EXTRA_ALL)

# include the dependency files

ifneq ($(noincludes),yes)
-include $(patsubst %.o,%.d,$(filter %.o,$(objs)))
endif

distclean: clean
	find -regex ".*\.d" -exec rm -f "{}" \;
	rm -f ./config.mk ./config.h ./config.err ./config.sed

clean:
	find -regex ".*\.\([aos]\|po\|so\(\.[0-9]\)?\)" -exec rm -f "{}" \;
	find -name "*~" -exec rm -f "{}" \;
	rm -f $(mans)
	rm -f ./gbsplay ./gbsinfo

install: all install-default $(EXTRA_INSTALL)

install-default:
	install -d $(bindir)
	install -d $(man1dir)
	install -d $(man5dir)
	install -d $(docdir)
	install -m 755 gbsplay   gbsinfo   $(bindir)
	install -m 644 gbsplay.1 gbsinfo.1 $(man1dir)
	install -m 644 gbsplayrc.5 $(man5dir)
	install -m 644 $(docs) $(docdir)

install-gbsxmms.so:
	install -d $(DESTDIR)$(XMMS_INPUT_PLUGIN_DIR)
	install -m 644 gbsxmms.so $(DESTDIR)$(XMMS_INPUT_PLUGIN_DIR)/gbsxmms.so

uninstall: uninstall-default $(EXTRA_UNINSTALL)

uninstall-default:
	rm -f $(bindir)/gbsplay    $(bindir)/gbsinfo
	rm -f $(man1dir)/gbsplay.1 $(man1dir)/gbsinfo.1
	rm -f $(man5dir)/gbsplayrc.5 $(man5dir)/gbsplayrc.5
	rm -rf $(docdir)
	-rmdir -p $(bindir)
	-rmdir -p $(man1dir)
	-rmdir -p $(man5dir)
	-rmdir -p $(docdir)

uninstall-gbsxmms.so:
	rm -f $(DESTDIR)$(XMMS_INPUT_PLUGIN_DIR)/gbsxmms.so
	-rmdir -p $(DESTDIR)$(XMMS_INPUT_PLUGIN_DIR)

dist:	distclean
	install -d ./$(DISTDIR)
	sed 's/^VERSION=.*/VERSION=$(VERSION)/' < configure > ./$(DISTDIR)/configure
	chmod 755 ./$(DISTDIR)/configure
	install -m 755 depend.sh ./$(DISTDIR)/
	install -m 644 Makefile ./$(DISTDIR)/
	install -m 644 *.c ./$(DISTDIR)/
	install -m 644 *.h ./$(DISTDIR)/
	install -m 644 $(mans_src) ./$(DISTDIR)/
	install -m 644 $(docs) INSTALL ./$(DISTDIR)/
	install -d ./$(DISTDIR)/contrib
	install -m 755 contrib/gbs2ogg.sh ./$(DISTDIR)/contrib
	tar -c $(DISTDIR)/ -vzf ../$(DISTDIR).tar.gz
	rm -rf ./$(DISTDIR)

ifeq ($(build_sharedlib),y)
LDFLAGS += -L. -lgbs

libgbs.so.1: $(objs_libgbspic)
	$(CC) -fPIC -shared -Wl,-soname=$@ -o $@ $+
	ln -s $@ libgbs.so

libgbs: libgbs.so.1
else
objs_gbsinfo    += libgbs.a
objs_gbsplay    += libgbs.a
objs_gbsxmms    += libgbspic.a

libgbs: libgbs.a libgbspic.a
endif

libgbspic.a: $(objs_libgbspic)
	$(AR) r $@ $+
libgbs.a: $(objs_libgbs)
	$(AR) r $@ $+
gbsinfo: $(objs_gbsinfo) libgbs
	$(CC) $(LDFLAGS) -o $@ $(objs_gbsinfo)
gbsplay: $(objs_gbsplay) libgbs
	$(CC) $(LDFLAGS) -o $@ $(objs_gbsplay) -lm

gbsxmms.so: $(objs_gbsxmms) libgbs
	$(CC) $(LDFLAGS) -shared -o $@ $(objs_gbsxmms) -lpthread

# rules for suffixes

.SUFFIXES: .i .s .po

.c.po:
	@echo CC $< -o $@
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<
.c.o:
	@echo CC $< -o $@
	@$(CC) $(CFLAGS) -c -o $@ $<
.c.i:
	$(CC) -E $(CFLAGS) -o $@ $<
.c.s:
	$(CC) -S $(CFLAGS) -fverbose-asm -o $@ $<

# rules for generated files

config.mk: configure
	./configure

%.d: %.c config.mk
	@echo DEP $< -o $@
	@./depend.sh $< > $@

%.1: %.in.1
	sed -f config.sed $< > $@

%.5: %.in.5
	sed -f config.sed $< > $@

