SDCC ?= sdcc
MAKEBIN ?= makebin
SDCCFLAGS := -mgbz80 --opt-code-size

all: test.rom

clean:
	rm *.ihx *.map *.rel *.noi *.rom *.sym *.lk *.lst *.asm

%.ihx: %.c
	$(SDCC) $(SDCCFLAGS) $<

%.rom: %.ihx
	$(MAKEBIN) -Z -yn TEST $< $@
