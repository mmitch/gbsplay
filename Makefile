# $Id: Makefile,v 1.3 2003/08/23 13:55:42 mitch Exp $

prefix = /usr/local
exec_prefix = ${prefix}

bindir = ${exec_prefix}/bin

CFLAGS := -Wall -Wstrict-prototypes -Os -g
LDFLAGS := -lm

SRCS := gbsplay.c

.PHONY: all distclean clean install
all: gbsplay

# determine the object files

OBJS := $(patsubst %.c,%.o,$(filter %.c,$(SRCS)))

# include the dependency files

%.d: %.c
	./depend.sh $< $(CFLAGS) > $@

include $(OBJS:.o=.d)

distclean: clean
	find -regex ".*\.d" -exec rm -f "{}" \;

clean:
	find -regex ".*\.\([aos]\|so\)" -exec rm -f "{}" \;

install: all
	install -d $(bindir)
	install -m 755 gbsplay $(bindir)/gbsplay

gbsplay: gbsplay.o 
	$(CC) $(LDFLAGS) -o $@ $<

.SUFFIXES: .i .s

.c.i:
	$(CC) -E $(CFLAGS) -o $@ $<
.c.s:
	$(CC) -S $(CFLAGS) -o $@ $<
