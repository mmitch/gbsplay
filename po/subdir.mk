# $Id: subdir.mk,v 1.3 2003/12/14 14:19:40 ranma Exp $

ifeq ($(have_xgettext),yes)

pos  := $(wildcard po/*.po)
mos  := $(patsubst %.po,%.mo,$(pos))
dsts += $(mos)

ifeq ($(MAKECMDGOALS),update-po)

.PHONY: update-po
update-po: po/gbsplay.pot $(pos) $(mos)

po/gbsplay.pot: $(shell find -name "*.c")
	xgettext -k_ -kN_ --language c $+ -o - | msgen -o $@ -

po/po.d: po/subdir.mk
	for i in po/*.po; do \
	echo "$$i: po/gbsplay.pot"; \
	echo "	msgmerge --no-location --no-wrap --no-fuzzy-matching -q -U \$$@ \$$< && touch \$$@"; \
	done > $@

-include po/po.d

endif

%.mo: %.po
	msgfmt -o $@ $<

endif
