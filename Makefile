# $Id: Makefile,v 1.18 2003/08/24 23:43:11 ranma Exp $

prefix = /usr/local
exec_prefix = ${prefix}

bindir = ${exec_prefix}/bin
mandir = ${prefix}/man
man1dir = $(mandir)/man1

CFLAGS := -Wall -Wstrict-prototypes -Os -fomit-frame-pointer -fPIC $(shell glib-config --cflags)
LDFLAGS :=

SRCS := gbcpu.c gbhw.c gbsplay.c gbsxmms.c

.PHONY: all distclean clean install dist

# determine the object files

OBJS := $(patsubst %.c,%.o,$(filter %.c,$(SRCS)))

all: $(OBJS) gbsplay gbsxmms.so

# include the dependency files

%.d: %.c
	CC=$(CC) ./depend.sh $< $(CFLAGS) > $@

include $(OBJS:.o=.d)

distclean: clean
	find -regex ".*\.d" -exec rm -f "{}" \;

clean:
	find -regex ".*\.\([aos]\|so\)" -exec rm -f "{}" \;
	find -name "*~" -exec rm -f "{}" \;
	rm -rf ./gbsplay

install: all
	install -d $(bindir)
	install -m 755 gbsplay $(bindir)/gbsplay
	install -d $(man1dir)
	install -m 644 gbsplay.1 $(man1dir)/gbsplay.1

uninstall:
	rm -f $(bindir)/gbsplay
	-rmdir $(bindir)
	rm -f $(man1dir)/gbsplay.1
	-rmdir $(man1dir)

dist:	distclean
	install -d ./gbsplay
	install -m 644 *.c ./gbsplay/
	install -m 644 *.h ./gbsplay/
	install -m 644 Makefile ./gbsplay/
	install -m 755 depend.sh ./gbsplay/
	install -m 644 gbsplay.1 ./gbsplay/
	install -m 644 README ./gbsplay/
	tar -c gbsplay/ -vzf ../gbsplay.tar.gz
	rm -rf ./gbsplay

gbsplay: gbsplay.o gbcpu.o gbhw.o
	$(CC) $(LDFLAGS) -o $@ gbsplay.o gbcpu.o gbhw.o -lm

gbsxmms.so: gbcpu.o gbhw.o gbsxmms.o
	$(CC) -shared $(LDFLAGS) -o $@ gbcpu.o gbhw.o gbsxmms.o -lpthread

.SUFFIXES: .i .s

.c.i:
	$(CC) -E $(CFLAGS) -o $@ $<
.c.s:
	$(CC) -S $(CFLAGS) -fverbose-asm -o $@ $<
