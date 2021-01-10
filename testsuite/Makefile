srcs  := $(wildcard *.c)
tests := $(srcs:.c=.test)

.PHONY: all default clean

all: default

default: clean $(tests)

clean:
	$(Q)rm -f $(tests)

%.test: %.c
	@echo TESTSUITE $<
# FIXME: create LIBGBSLDFLAGS und libgbs_objs in main Makefile to include here
	$(Q)$(HOSTCC) -o $@$(binsuffix) $< ../libgbs.a -lm -lz
	$(Q)./$@$(binsuffix)
	$(Q)rm ./$@$(binsuffix)
