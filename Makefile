CFLAGS := -Wall -Wstrict-prototypes -Os -g
LDFLAGS := -lm

SRCS := gbsplay.c

.PHONY: all distclean clean
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

gbsplay: gbsplay.o 
	$(CC) $(LDFLAGS) -o $@ $<

.SUFFIXES: .i .s

.c.i:
	$(CC) -E $(CFLAGS) -o $@ $<
.c.s:
	$(CC) -S $(CFLAGS) -o $@ $<
