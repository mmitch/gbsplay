redo-ifchange config
. ./config.sh
# TODO:splint @(test -x "`which $(SPLINT)`" && $(SPLINT) $(SPLINTFLAGS) $<) || true
echo "$BUILDCC $GBSCFLAGS -c -o \$3 \$1.c" > $3
chmod a+x $3
