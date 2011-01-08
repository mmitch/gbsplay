redo-ifchange config
. ./config.sh
## TODO: why is splint not checked in configure like everything else?
if test -x "`which $SPLINT`" ; then
    echo "$SPLINT $SPLINTFLAGS \$1.c >&2" > $3
fi
echo "$BUILDCC $GBSCFLAGS -c -o \$3 \$1.c" >> $3
chmod a+x $3
