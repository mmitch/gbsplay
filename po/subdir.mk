# $Id: subdir.mk,v 1.2 2003/12/14 00:31:48 ranma Exp $

ifeq ($(have_xgettext),yes)

pos  := $(wildcard po/*.po)
mos  := $(patsubst %.po,%.mo,$(pos))
dsts += po/gbsplay.pot $(pos) $(mos)

po/gbsplay.pot: $(shell find -name "*.c")
	xgettext -k_ -kN_ --language c $+ -o - | msgen -o $@ -

po/po.d: po/subdir.mk
	for i in po/*.po; do \
	echo "$$i: po/gbsplay.pot"; \
	echo "	msgmerge -U \$$@ \$$< && touch \$$@"; \
	done > $@

-include po/po.d

%.mo: %.po
	msgfmt -o $@ $<

endif
