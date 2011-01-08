all:
	@echo "This is not the Makefile you're looking for."
	@echo ""
	@echo "The build process of gbsplay has been switched to redo, see https://github.com/apenwarr/redo"
	@echo "The following targets are available:"
	@echo "  redo all        - configure and build"
	@echo "  redo install    - install"
	@echo "  redo uninstall  - uninstall (only works properly without configuration changes)"
	@echo "  redo clean      - clean up"
	@echo "  redo distclean  - completely start over"
	@echo "  redo dist       - export a clean .tar.gz"

.PHONY: all default distclean clean install dist

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
	$(BUILDCC) -fpic -shared -Wl,-O1 -Wl,-soname=$@ -Wl,--version-script,$@.ver -o $@ $(objs_libgbspic)
	ln -fs $@ libgbs.so

libgbs: libgbs.so.1
	touch libgbs

libgbspic: libgbs.so.1
	touch libgbspic

endif
else
## EXPORTED
endif # use_sharedlibs

install: all install-default $(EXTRA_INSTALL)

install-default:
	install -d $(bindir)
	install -d $(man1dir)
	install -d $(man5dir)
	install -d $(docdir)
	install -d $(exampledir)
	install -m 755 $(gbsplaybin) $(gbsinfobin) $(bindir)
	install -m 644 gbsplay.1 gbsinfo.1 $(man1dir)
	install -m 644 gbsplayrc.5 $(man5dir)
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
	rm -f $(man5dir)/gbsplayrc.5 $(man5dir)/gbsplayrc.5	
	-rmdir -p $(man5dir)
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

TESTOPTS := -r 44100 -t 30 -f 0 -g 0 -T 0

test: gbsplay
	@MD5=`LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./gbsplay -E b -o stdout $(TESTOPTS) examples/nightmode.gbs 1 < /dev/null | md5sum | cut -f1 -d\ `; \
	EXPECT="5269fdada196a6b67d947428ea3ca934"; \
	if [ "$$MD5" = "$$EXPECT" ]; then \
		echo "Bigendian output ok"; \
	else \
		echo "Bigendian output failed"; \
		echo "  Expected: $$EXPECT"; \
		echo "  Got:      $$MD5" ; \
		exit 1; \
	fi
	@MD5=`LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./gbsplay -E l -o stdout $(TESTOPTS) examples/nightmode.gbs 1 < /dev/null | md5sum | cut -f1 -d\ `; \
	EXPECT="3c005a70135621d8eb3e0dc20982eba8"; \
	if [ "$$MD5" = "$$EXPECT" ]; then \
		echo "Littleendian output ok"; \
	else \
		echo "Littleendian output failed"; \
		echo "  Expected: $$EXPECT"; \
		echo "  Got:      $$MD5" ; \
		exit 1; \
	fi

gbsxmms.so: $(objs_gbsxmms) libgbspic gbsxmms.so.ver
	$(BUILDCC) -shared -fpic -Wl,--version-script,$@.ver -o $@ $(objs_gbsxmms) $(GBSLDFLAGS) $(PTHREAD)

# rules for suffixes

.SUFFIXES: .i .s .lo

.c.lo:
	@echo CC $< -o $@
	@$(BUILDCC) $(GBSCFLAGS) -fpic -c -o $@ $<
.c.i:
	$(BUILDCC) -E $(GBSCFLAGS) -o $@ $<
.c.s:
	$(BUILDCC) -S $(GBSCFLAGS) -fverbose-asm -o $@ $<
