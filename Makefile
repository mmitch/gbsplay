# $Id: Makefile,v 1.99 2006/01/24 22:19:33 mitch Exp $

.PHONY: all default distclean clean install dist

all: default

noincludes  := $(patsubst clean,yes,$(patsubst distclean,yes,$(MAKECMDGOALS)))

prefix      := /usr/local
exec_prefix := $(prefix)

bindir      := $(exec_prefix)/bin
libdir      := $(exec_prefix)/lib
mandir      := $(prefix)/man
docdir      := $(prefix)/share/doc/gbsplay
localedir   := $(prefix)/share/locale

GBSCFLAGS  := -Wall
GBSLDFLAGS := -Wl,-O1
GBSPLAYLDFLAGS :=

ifneq ($(noincludes),yes)
-include config.mk
endif

XMMSPREFIX  :=
DESTDIR     :=

prefix      := $(DESTDIR)$(prefix)
exec_prefix := $(DESTDIR)$(exec_prefix)
bindir      := $(DESTDIR)$(bindir)
mandir      := $(DESTDIR)$(mandir)
docdir      := $(DESTDIR)$(docdir)
localedir   := $(DESTDIR)$(localedir)

xmmsdir     := $(DESTDIR)$(XMMSPREFIX)$(XMMS_INPUT_PLUGIN_DIR)

man1dir     := $(mandir)/man1
man5dir     := $(mandir)/man5
contribdir  := $(docdir)/contrib
exampledir  := $(docdir)/examples

DISTDIR := gbsplay-$(VERSION)

GBSCFLAGS  += $(EXTRA_CFLAGS)
GBSLDFLAGS += $(EXTRA_LDFLAGS)

export CC GBSCFLAGS GBSLDFLAGS

docs           := README HISTORY COPYRIGHT
contribs       := contrib/gbs2ogg.sh
examples       := examples/nightmode.gbs examples/gbsplayrc_sample

mans           := gbsplay.1    gbsinfo.1    gbsplayrc.5
mans_src       := gbsplay.in.1 gbsinfo.in.1 gbsplayrc.in.5

objs_libgbspic := gbcpu.lo gbhw.lo gbs.lo cfgparser.lo crc32.lo
objs_libgbs    := gbcpu.o  gbhw.o  gbs.o  cfgparser.o  crc32.o
objs_gbsplay   := gbsplay.o util.o plugout.o
objs_gbsinfo   := gbsinfo.o
objs_gbsxmms   := gbsxmms.lo

# gbsplay output plugins
ifeq ($(plugout_devdsp),yes)
objs_gbsplay += plugout_devdsp.o
endif
ifeq ($(plugout_nas),yes)
objs_gbsplay += plugout_nas.o
GBSPLAYLDFLAGS += -laudio $(libaudio_flags)
endif
ifeq ($(plugout_stdout),yes)
objs_gbsplay += plugout_stdout.o
endif

# Cygwin automatically adds .exe to binaries.
# We should notice that or we can't rm the files later!
gbsplaybin     := gbsplay
gbsinfobin     := gbsinfo
ifeq ($(cygwin_build),yes)
gbsplaybin     :=$(gbsplaybin).exe
gbsinfobin     :=$(gbsinfobin).exe
endif

ifeq ($(use_sharedlibgbs),yes)
GBSLDFLAGS += -L. -lgbs
objs += $(objs_libgbspic)
ifeq ($(cygwin_build),yes)
EXTRA_INSTALL += install-cyggbs-1.dll
EXTRA_UNINSTALL += uninstall-cyggbs-1.dll


install-cyggbs-1.dll:
	install -d $(bindir)
	install -d $(libdir)
	install -m 644 cyggbs-1.dll $(bindir)/cyggbs-1.dll
	install -m 644 libgbs.dll.a $(libdir)/libgbs.dll.a

uninstall-cyggbs-1.dll:
	rm -f $(bindir)/cyggbs-1.dll
	rm -f $(libdir)/libgbs.dll.a
	-rmdir -p $(libdir)


cyggbs-1.dll: $(objs_libgbspic) libgbs.so.1.ver
	$(CC) -fpic -shared -Wl,-O1 -Wl,-soname=$@ -Wl,--version-script,libgbs.so.1.ver -o $@ $(objs_libgbspic) $(EXTRA_LDFLAGS)

libgbs.dll.a:
	dlltool --input-def libgbs.def --dllname cyggbs-1.dll --output-lib libgbs.dll.a -k

libgbs: cyggbs-1.dll libgbs.dll.a
	touch libgbs

libgbspic: cyggbs-1.dll libgbs.dll.a
	touch libgbspic
else
EXTRA_INSTALL += install-libgbs.so.1
EXTRA_UNINSTALL += uninstall-libgbs.so.1


install-libgbs.so.1:
	install -d $(libdir)
	install -m 644 libgbs.so.1 $(libdir)/libgbs.so.1

uninstall-libgbs.so.1:
	rm -f $(libdir)/libgbs.so.1
	-rmdir -p $(libdir)


libgbs.so.1: $(objs_libgbspic) libgbs.so.1.ver
	$(CC) -fpic -shared -Wl,-O1 -Wl,-soname=$@ -Wl,--version-script,$@.ver -o $@ $(objs_libgbspic)
	ln -fs $@ libgbs.so

libgbs: libgbs.so.1
	touch libgbs

libgbspic: libgbs.so.1
	touch libgbspic
endif
else
objs += $(objs_libgbs)
objs_gbsplay += libgbs.a
objs_gbsinfo += libgbs.a
ifeq ($(build_xmmsplugin),yes)
objs += $(objs_libgbspic)
objs_gbsxmms += libgbspic.a
endif # build_xmmsplugin

libgbs: libgbs.a
	touch libgbs

libgbspic: libgbspic.a
	touch libgbspic
endif # use_sharedlibs

objs += $(objs_gbsplay) $(objs_gbsinfo)
dsts += gbsplay gbsinfo

ifeq ($(build_xmmsplugin),yes)
objs += $(objs_gbsxmms)
dsts += gbsxmms.so
endif

# include the rules for each subdir
include $(shell find . -type f -name "subdir.mk")

default: config.mk $(objs) $(dsts) $(mans) $(EXTRA_ALL) test

# include the dependency files

ifneq ($(noincludes),yes)
deps := $(patsubst %.o,%.d,$(filter %.o,$(objs)))
deps += $(patsubst %.lo,%.d,$(filter %.lo,$(objs)))
-include $(deps)
endif

distclean: clean
	find . -regex ".*\.d" -exec rm -f "{}" \;
	rm -f ./config.mk ./config.h ./config.err ./config.sed

clean:
	find . -regex ".*\.\([aos]\|lo\|mo\|pot\|so\(\.[0-9]\)?\)" -exec rm -f "{}" \;
	find . -name "*~" -exec rm -f "{}" \;
	rm -f libgbs libgbspic
	rm -f $(mans)
	rm -f $(gbsplaybin) $(gbsinfobin)

install: all install-default $(EXTRA_INSTALL)

install-default:
	install -d $(bindir)
	install -d $(man1dir)
	install -d $(man5dir)
	install -d $(docdir)
	install -d $(contribdir)
	install -d $(exampledir)
	install -m 755 $(gbsplaybin) $(gbsinfobin) $(bindir)
	install -m 644 gbsplay.1 gbsinfo.1 $(man1dir)
	install -m 644 gbsplayrc.5 $(man5dir)
	install -m 644 $(docs) $(docdir)
	install -m 644 $(contribs) $(contribdir)
	install -m 644 $(examples) $(exampledir)
	for i in $(mos); do \
		base=`basename $$i`; \
		install -d $(localedir)/$${base%.mo}/LC_MESSAGES; \
		install -m 644 $$i $(localedir)/$${base%.mo}/LC_MESSAGES/gbsplay.mo; \
	done

install-gbsxmms.so:
	install -d $(xmmsdir)
	install -m 644 gbsxmms.so $(xmmsdir)/gbsxmms.so

uninstall: uninstall-default $(EXTRA_UNINSTALL)

uninstall-default:
	rm -f $(bindir)/$(gbsplaybin) $(bindir)/$(gbsinfobin)
	-rmdir -p $(bindir)
	rm -f $(man1dir)/gbsplay.1 $(man1dir)/gbsinfo.1
	-rmdir -p $(man1dir)
	rm -f $(man5dir)/gbsplayrc.5 $(man5dir)/gbsplayrc.5	
	-rmdir -p $(man5dir)
	rm -rf $(contribdir)
	-rmdir -p $(contribdir)
	rm -rf $(exampledir)
	-rmdir -p $(exampledir)
	rm -rf $(docdir)
	-mkdir -p $(docdir)
	-rmdir -p $(docdir)
	-for i in $(mos); do \
		base=`basename $$i`; \
		rm -f $(localedir)/$${base%.mo}/LC_MESSAGES/gbsplay.mo; \
		rmdir -p $(localedir)/$${base%.mo}/LC_MESSAGES; \
	done

uninstall-gbsxmms.so:
	rm -f $(xmmsdir)/gbsxmms.so
	-rmdir -p $(xmmsdir)

dist:	distclean
	install -d ./$(DISTDIR)
	sed 's/^VERSION=.*/VERSION=$(VERSION)/' < configure > ./$(DISTDIR)/configure
	chmod 755 ./$(DISTDIR)/configure
	install -m 755 depend.sh ./$(DISTDIR)/
	install -m 644 Makefile ./$(DISTDIR)/
	install -m 644 *.c ./$(DISTDIR)/
	install -m 644 *.h ./$(DISTDIR)/
	install -m 644 *.ver ./$(DISTDIR)/
	install -m 644 $(mans_src) ./$(DISTDIR)/
	install -m 644 $(docs) INSTALL CODINGSTYLE ./$(DISTDIR)/
	install -d ./$(DISTDIR)/examples
	install -m 644 $(examples) ./$(DISTDIR)/examples
	install -d ./$(DISTDIR)/contrib
	install -m 644 $(contribs) ./$(DISTDIR)/contrib
	install -d ./$(DISTDIR)/po
	install -m 644 po/*.po ./$(DISTDIR)/po
	install -m 644 po/subdir.mk ./$(DISTDIR)/po
	tar -cvzf ../$(DISTDIR).tar.gz $(DISTDIR)/ 
	rm -rf ./$(DISTDIR)

TESTOPTS := -r 44100 -t 30 -f 0 -g 0 -T 0

test: gbsplay
	@MD5=`LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./gbsplay -E b -o stdout $(TESTOPTS) examples/nightmode.gbs 1 | md5sum | cut -f1 -d\ `; \
	EXPECT="3e0b21573e742101581b503da3ddf2f5"; \
	if [ "$$MD5" = "$$EXPECT" ]; then \
		echo "Bigendian output ok"; \
	else \
		echo "Bigendian output failed"; \
		echo "  Expected: $$EXPECT"; \
		echo "  Got:      $$MD5" ; \
		exit 1; \
	fi
	@MD5=`LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./gbsplay -E l -o stdout $(TESTOPTS) examples/nightmode.gbs 1 | md5sum | cut -f1 -d\ `; \
	EXPECT="3e540a0bddc00cb1e0b7c4bf1a3708d7"; \
	if [ "$$MD5" = "$$EXPECT" ]; then \
		echo "Littleendian output ok"; \
	else \
		echo "Littleendian output failed"; \
		echo "  Expected: $$EXPECT"; \
		echo "  Got:      $$MD5" ; \
		exit 1; \
	fi

libgbspic.a: $(objs_libgbspic)
	$(AR) r $@ $+
libgbs.a: $(objs_libgbs)
	$(AR) r $@ $+
gbsinfo: $(objs_gbsinfo) libgbs
	$(CC) -o $(gbsinfobin) $(objs_gbsinfo) $(GBSLDFLAGS)
gbsplay: $(objs_gbsplay) libgbs
	$(CC) -o $(gbsplaybin) $(objs_gbsplay) $(GBSLDFLAGS) $(GBSPLAYLDFLAGS) -lm

gbsxmms.so: $(objs_gbsxmms) libgbspic gbsxmms.so.ver
	$(CC) -shared -fpic -Wl,--version-script,$@.ver -o $@ $(objs_gbsxmms) $(GBSLDFLAGS) $(PTHREAD)

# rules for suffixes

.SUFFIXES: .i .s .lo

.c.lo:
	@echo CC $< -o $@
	@$(CC) $(GBSCFLAGS) -fpic -c -o $@ $<
.c.o:
	@echo CC $< -o $@
	@$(CC) $(GBSCFLAGS) -c -o $@ $<
.c.i:
	$(CC) -E $(GBSCFLAGS) -o $@ $<
.c.s:
	$(CC) -S $(GBSCFLAGS) -fverbose-asm -o $@ $<

# rules for generated files

config.mk: configure
	./configure

%.d: %.c config.mk
	@echo DEP $< -o $@
	@./depend.sh $< config.mk > $@ || rm -f $@

%.1: %.in.1
	sed -f config.sed $< > $@

%.5: %.in.5
	sed -f config.sed $< > $@
