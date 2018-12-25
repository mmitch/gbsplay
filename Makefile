.PHONY: all default distclean clean install dist

ifeq ("$(origin V)", "command line")
  VERBOSE = $(V)
endif
ifndef VERBOSE
  VERBOSE = 0
endif
ifeq ($(VERBOSE),1)
  Q =
else
  Q = @
endif

all: default

noincludes  := $(patsubst clean,yes,$(patsubst distclean,yes,$(MAKECMDGOALS)))

prefix      := /usr/local
exec_prefix := $(prefix)

bindir      := $(exec_prefix)/bin
libdir      := $(exec_prefix)/lib
mandir      := $(prefix)/man
docdir      := $(prefix)/share/doc/gbsplay
localedir   := $(prefix)/share/locale
mimedir     := $(prefix)/share/mime
appdir      := $(prefix)/share/applications

SPLINT := splint

SPLINTFLAGS := \
	+quiet \
	-exportlocal \
	-unrecog \
	-immediatetrans \
	-fullinitblock \
	-namechecks \
	-preproc \
	-fcnuse \
	-predboolint \
	-boolops \
	-formatconst \
	-type \
	+unixlib \
	+boolint \
	+matchanyintegral \
	+charint \
	-predboolothers \
	-shiftnegative \
	-shiftimplementation

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
mimedir     := $(DESTDIR)$(mimedir)
appdir      := $(DESTDIR)$(appdir)

xmmsdir     := $(DESTDIR)$(XMMSPREFIX)$(XMMS_INPUT_PLUGIN_DIR)

man1dir     := $(mandir)/man1
man5dir     := $(mandir)/man5
contribdir  := $(docdir)/contrib
exampledir  := $(docdir)/examples

DISTDIR := gbsplay-$(VERSION)

GBSCFLAGS  := $(EXTRA_CFLAGS)
GBSLDFLAGS := $(EXTRA_LDFLAGS)
# Additional ldflags for the gbsplay executable
GBSPLAYLDFLAGS :=

export Q VERBOSE CC HOSTCC BUILDCC GBSCFLAGS GBSLDFLAGS

docs               := README HISTORY COPYRIGHT
docs-dist          := INSTALL CODINGSTYLE TESTSUITE gbsformat.txt
contribs           := contrib/gbs2ogg.sh contrib/gbsplay.bashcompletion
examples           := examples/nightmode.gbs examples/gbsplayrc_sample

mans               := man/gbsplay.1    man/gbsinfo.1    man/gbsplayrc.5
mans_src           := man/gbsplay.in.1 man/gbsinfo.in.1 man/gbsplayrc.in.5

objs_libgbspic     := gbcpu.lo gbhw.lo gbs.lo cfgparser.lo crc32.lo
objs_libgbs        := gbcpu.o  gbhw.o  gbs.o  cfgparser.o  crc32.o
objs_gbsplay       := gbsplay.o util.o plugout.o
objs_gbsinfo       := gbsinfo.o
objs_gbsxmms       := gbsxmms.lo
objs_test_gbs      := test_gbs.o
objs_gen_impulse_h := gen_impulse_h.ho impulsegen.ho

tests              := util.test impulsegen.test

# gbsplay output plugins
ifeq ($(plugout_devdsp),yes)
objs_gbsplay += plugout_devdsp.o
endif
ifeq ($(plugout_alsa),yes)
objs_gbsplay += plugout_alsa.o
GBSPLAYLDFLAGS += -lasound $(libaudio_flags)
endif
ifeq ($(plugout_nas),yes)
objs_gbsplay += plugout_nas.o
GBSPLAYLDFLAGS += -laudio $(libaudio_flags)
endif
ifeq ($(plugout_stdout),yes)
objs_gbsplay += plugout_stdout.o
endif
ifeq ($(plugout_midi),yes)
objs_gbsplay += plugout_midi.o
endif
ifeq ($(plugout_altmidi),yes)
objs_gbsplay += plugout_altmidi.o
endif
ifeq ($(plugout_pulse),yes)
objs_gbsplay += plugout_pulse.o
GBSPLAYLDFLAGS += -lpulse-simple -lpulse
endif
ifeq ($(plugout_dsound),yes)
objs_gbsplay += plugout_dsound.o
GBSPLAYLDFLAGS += -ldsound $(libdsound_flags)
endif
ifeq ($(plugout_iodumper),yes)
objs_gbsplay += plugout_iodumper.o
endif

# install contrib files?
ifeq ($(build_contrib),yes)
EXTRA_INSTALL += install-contrib
EXTRA_UNINSTALL += uninstall-contrib
endif

# test built binary?
ifeq ($(build_test),yes)
TEST_TARGETS += test
endif

# Cygwin automatically adds .exe to binaries.
# We should notice that or we can't rm the files later!
ifeq ($(cygwin_build),yes)
binsuffix         := .exe
else
binsuffix         :=
endif
gbsplaybin        := gbsplay$(binsuffix)
gbsinfobin        := gbsinfo$(binsuffix)
test_gbsbin       := test_gbs$(binsuffix)
gen_impulse_h_bin := gen_impulse_h$(binsuffix)

ifeq ($(use_sharedlibgbs),yes)
GBSLDFLAGS += -L. -lgbs
objs += $(objs_libgbspic)

libgbs.so.1.ver: libgbs_whitelist.txt gen_linkercfg.sh
	./gen_linkercfg.sh libgbs_whitelist.txt $@ ver

libgbs.def: libgbs_whitelist.txt gen_linkercfg.sh
	./gen_linkercfg.sh libgbs_whitelist.txt $@ def

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
	$(CC) -fpic -shared -Wl,-soname=$@ -Wl,--version-script,libgbs.so.1.ver -o $@ $(objs_libgbspic) $(EXTRA_LDFLAGS)

libgbs.dll.a: libgbs.def
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
	$(BUILDCC) -fpic -shared -Wl,-soname=$@ -Wl,--version-script,$@.ver -o $@ $(objs_libgbspic) -lm
	ln -fs $@ libgbs.so

libgbs: libgbs.so.1
	touch libgbs

libgbspic: libgbs.so.1
	touch libgbspic
endif
else
GBSLDFLAGS += -lm
objs += $(objs_libgbs)
objs_gbsplay += libgbs.a
objs_gbsinfo += libgbs.a
objs_test_gbs += libgbs.a
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

default: config.mk $(objs) $(dsts) $(mans) $(EXTRA_ALL) $(TEST_TARGETS)

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
	find . -regex ".*\.\([aos]\|ho\|lo\|mo\|pot\|test\(\.exe\)?\|so\(\.[0-9]\)?\)" -exec rm -f "{}" \;
	find . -name "*~" -exec rm -f "{}" \;
	rm -f libgbs libgbspic libgbs.def libgbs.so.1.ver
	rm -f $(mans)
	rm -f $(gbsplaybin) $(gbsinfobin)
	rm -f $(test_gbsbin)
	rm -f $(gen_impulse_h_bin) impulse.h

install: all install-default $(EXTRA_INSTALL)

install-default:
	install -d $(bindir)
	install -d $(man1dir)
	install -d $(man5dir)
	install -d $(docdir)
	install -d $(exampledir)
	install -d $(mimedir)/packages
	install -d $(appdir)
	install -m 755 $(gbsplaybin) $(gbsinfobin) $(bindir)
	install -m 644 man/gbsplay.1 man/gbsinfo.1 $(man1dir)
	install -m 644 man/gbsplayrc.5 $(man5dir)
	install -m 644 mime/gbsplay.xml $(mimedir)/packages
	-update-mime-database $(mimedir)
	install -m 644 desktop/gbsplay.desktop $(appdir)
	-update-desktop-database $(appdir)
	install -m 644 $(docs) $(docdir)
	install -m 644 $(examples) $(exampledir)
	for i in $(mos); do \
		base=`basename $$i`; \
		install -d $(localedir)/$${base%.mo}/LC_MESSAGES; \
		install -m 644 $$i $(localedir)/$${base%.mo}/LC_MESSAGES/gbsplay.mo; \
	done

install-contrib:
	install -d $(contribdir)
	install -m 644 $(contribs) $(contribdir)

install-gbsxmms.so:
	install -d $(xmmsdir)
	install -m 644 gbsxmms.so $(xmmsdir)/gbsxmms.so

uninstall: uninstall-default $(EXTRA_UNINSTALL)

uninstall-default:
	rm -f $(bindir)/$(gbsplaybin) $(bindir)/$(gbsinfobin)
	-rmdir -p $(bindir)
	rm -f $(man1dir)/gbsplay.1 $(man1dir)/gbsinfo.1
	-rmdir -p $(man1dir)
	rm -f $(man5dir)/gbsplayrc.5
	-rmdir -p $(man5dir)
	rm -f $(mimedir)/packages/gbsplay.xml
	-update-mime-database $(mimedir)
	-rmdir -p $(mimedir)/packages
	rm -f $(appdir)/gbsplay.desktop
	-update-desktop-database $(appdir)
	-rmdir -p $(appdir)
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

uninstall-contrib:
	rm -rf $(contribdir)
	-rmdir -p $(contribdir)

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
	install -d ./$(DISTDIR)/man
	install -m 644 $(mans_src) ./$(DISTDIR)/man
	install -m 644 $(docs) $(docs-dist) ./$(DISTDIR)/
	install -d ./$(DISTDIR)/examples
	install -m 644 $(examples) ./$(DISTDIR)/examples
	install -d ./$(DISTDIR)/contrib
	install -m 644 $(contribs) ./$(DISTDIR)/contrib
	install -d ./$(DISTDIR)/po
	install -m 644 po/*.po ./$(DISTDIR)/po
	install -m 644 po/subdir.mk ./$(DISTDIR)/po
	install -m 644 .gitignore ./$(DISTDIR)/
	install -d ./$(DISTDIR)/mime
	install -m 644 mime/* ./$(DISTDIR)/mime
	install -d ./$(DISTDIR)/desktop
	install -m 644 desktop/* ./$(DISTDIR)/desktop
	tar -cvzf ../$(DISTDIR).tar.gz $(DISTDIR)/ 
	rm -rf ./$(DISTDIR)

TESTOPTS := -r 44100 -t 30 -f 0 -g 0 -T 0 -H off

test: gbsplay $(tests) test_gbs
	@echo Verifying output correctness for examples/nightmode.gbs:
	$(Q)MD5=`LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./gbsplay -c examples/gbsplayrc_sample -E b -o stdout $(TESTOPTS) examples/nightmode.gbs 1 < /dev/null | (md5sum || md5 -r) | cut -f1 -d\ `; \
	EXPECT="7e2861c25ca6719d4b870240d97a8890"; \
	if [ "$$MD5" = "$$EXPECT" ]; then \
		echo "Bigendian output ok"; \
	else \
		echo "Bigendian output failed"; \
		echo "  Expected: $$EXPECT"; \
		echo "  Got:      $$MD5" ; \
		exit 1; \
	fi
	$(Q)MD5=`LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./gbsplay -c examples/gbsplayrc_sample -E l -o stdout $(TESTOPTS) examples/nightmode.gbs 1 < /dev/null | (md5sum || md5 -r) | cut -f1 -d\ `; \
	EXPECT="0ff4d67e18f78a582d0929881a0ca94b"; \
	if [ "$$MD5" = "$$EXPECT" ]; then \
		echo "Littleendian output ok"; \
	else \
		echo "Littleendian output failed"; \
		echo "  Expected: $$EXPECT"; \
		echo "  Got:      $$MD5" ; \
		exit 1; \
	fi

$(gen_impulse_h_bin): $(objs_gen_impulse_h)
	$(HOSTCC) -o $(gen_impulse_h_bin) $(objs_gen_impulse_h) -lm
impulse.h: $(gen_impulse_h_bin)
	$(Q)./$(gen_impulse_h_bin) > $@
gbhw.o: impulse.h
gbhw.lo: impulse.h

libgbspic.a: $(objs_libgbspic)
	$(AR) r $@ $+
libgbs.a: $(objs_libgbs)
	$(AR) r $@ $+
gbsinfo: $(objs_gbsinfo) libgbs
	$(BUILDCC) -o $(gbsinfobin) $(objs_gbsinfo) $(GBSLDFLAGS)
gbsplay: $(objs_gbsplay) libgbs
	$(BUILDCC) -o $(gbsplaybin) $(objs_gbsplay) $(GBSLDFLAGS) $(GBSPLAYLDFLAGS) -lm
test_gbs: $(objs_test_gbs) libgbs
	$(BUILDCC) -o $(test_gbsbin) $(objs_test_gbs) $(GBSLDFLAGS)

gbsxmms.so: $(objs_gbsxmms) libgbspic gbsxmms.so.ver
	$(BUILDCC) -shared -fpic -Wl,--version-script,$@.ver -o $@ $(objs_gbsxmms) $(GBSLDFLAGS) $(PTHREAD)

# rules for suffixes

.SUFFIXES: .i .s .lo .ho

.c.lo:
	@echo CC $< -o $@
	$(Q)$(BUILDCC) $(GBSCFLAGS) -fpic -c -o $@ $<
.c.o:
	@echo CC $< -o $@
	$(Q)(test -x "`which $(SPLINT)`" && $(SPLINT) $(SPLINTFLAGS) $<) || true
	$(Q)$(BUILDCC) $(GBSCFLAGS) -fpie -c -o $@ $<
.c.ho:
	@echo HOSTCC $< -o $@
	$(Q)$(HOSTCC) $(GBSCFLAGS) -fpie -c -o $@ $<

.c.i:
	$(BUILDCC) -E $(GBSCFLAGS) -o $@ $<
.c.s:
	$(BUILDCC) -S $(GBSCFLAGS) -fverbose-asm -o $@ $<

# rules for generated files

config.mk: configure
	./configure

%.test: %.c
	@echo TEST $<
	$(Q)$(HOSTCC) -DENABLE_TEST=1 -o $@$(binsuffix) $< -lm
	$(Q)./$@$(binsuffix)
	$(Q)rm ./$@$(binsuffix)

%.d: %.c config.mk
	@echo DEP $< -o $@
	$(Q)./depend.sh $< config.mk > $@ || rm -f $@

%.1: %.in.1 config.sed
	sed -f config.sed $< > $@

%.5: %.in.5 config.sed
	sed -f config.sed $< > $@
