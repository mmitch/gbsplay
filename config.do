redo-ifchange configure
./configure

. ./config.sh

# Makefile imported madness!
# TODO: nearly everything behaves differently when set in ENV before call!
# this is not redo-compatible I think!

TESTOPTS="-r 44100 -t 30 -f 0 -g 0 -T 0"

prefix=/usr/local
exec_prefix=$prefix

bindir=$exec_prefix/bin
libdir=$exec_prefix/lib
mandir=$prefix/man
docdir=$prefix/share/doc/gbsplay
localedir=$prefix/share/locale

SPLINT=splint

SPLINTFLAGS="\
	+quiet \
	-exportlocal \
	-unrecog \
	-immediatetrans \
	-fullinitblock \
	-namechecks \
	-preproc \
	-fcnuse \
	-predboolint \
	-boolops \
	-formatconst \
	-type \
	+unixlib \
	+boolint \
	+matchanyintegral \
	+charint \
	-predboolothers \
	-shiftnegative \
	-shiftimplementation"
GBSCFLAGS="-Wall -fsigned-char -D_FORTIFY_SOURCE=2"
GBSLDFLAGS="-Wl,-O1 -lm"
GBSPLAYLDFLAGS=

XMMSPREFIX=
DESTDIR=

prefix=$DESTDIR$prefix
exec_prefix=$DESTDIR$exec_prefix
bindir=$DESTDIR$bindir
mandir=$DESTDIR$mandir
docdir=$DESTDIR$docdir
localedir=$DESTDIR$localedir

xmmsdir=$DESTDIR$XMMSPREFIX$XMMS_INPUT_PLUGIN_DIR

man1dir=$mandir/man1
man5dir=$mandir/man5
contribdir=$docdir/contrib
exampledir=$docdir/examples

DISTDIR=gbsplay-$VERSION

GBSCFLAGS="$GBSCFLAGS $EXTRA_CFLAGS"
GBSLDFLAGS="$GBSLDFLAGS $EXTRA_LDFLAGS"

docs="README HISTORY COPYRIGHT"
contribs="contrib/gbs2ogg.sh contrib/gbsplay.bashcompletion"
examples="examples/nightmode.gbs examples/gbsplayrc_sample"

mans_1="gbsplay.1 gbsinfo.1"
mans_5="gbsplayrc.5"
mans="$mans_1 $mans_5"
mans_src="gbsplay.in.1 gbsinfo.in.1 gbsplayrc.in.5"

objs_libgbspic="gbcpu.lo gbhw.lo gbs.lo cfgparser.lo crc32.lo impulsegen.lo"
objs_libgbs="   gbcpu.o  gbhw.o  gbs.o  cfgparser.o  crc32.o  impulsegen.o"
objs_gbsplay="gbsplay.o util.o plugout.o"
objs_gbsinfo=gbsinfo.o
objs_gbsxmms=gbsxmms.lo

# gbsplay output plugins
if [ $plugout_devdsp = yes ] ; then
    objs_gbsplay="$objs_gbsplay plugout_devdsp.o"
fi
if [ $plugout_alsa = yes ] ; then
    objs_gbsplay="$objs_gbsplay plugout_alsa.o"
    GBSPLAYLDFLAGS="$GBSPLAYLDFLAGS -lasound $libaudio_flags"
fi
if [ $plugout_nas = yes ] ; then
    objs_gbsplay="$objs_gbsplay plugout_nas.o"
    GBSPLAYLDFLAGS="$GBSPLAYLDFLAGS -laudio $libaudio_flags"
fi
if [ $plugout_stdout = yes ] ; then
    objs_gbsplay="$objs_gbsplay plugout_stdout.o"
fi
if [ $plugout_midi = yes ] ; then
    objs_gbsplay="$objs_gbsplay plugout_midi.o"
fi

# install contrib files?
if [ $build_contrib = yes ] ; then
    EXTRA_INSTALL="$EXTRA_INSTALL install-contrib"
    EXTRA_UNINSTALL="$EXTRA_UNINSTALL uninstall-contrib"
fi

# test built binary?
if [ $build_test = yes ] ; then
    TEST_TARGETS="$TEST_TARGETS test"
fi

# Cygwin automatically adds .exe to binaries.
# We should notice that or we can't rm the files later!
gbsplaybin=gbsplay
gbsinfobin=gbsinfo
if [ $cygwin_build = yes ] ; then
    gbsplaybin=$gbsplaybin.exe
    gbsinfobin=$gbsinfobin.exe
fi


### IF no shared 

objs="$objs $objs_libgbs"
objs_gbsplay="$objs_gbsplay libgbs.a"
objs_gbsinfo="$objs_gbsinfo libgbs.a"
if [ $build_xmmsplugin = yes ] ; then
    objs="$objs $objs_libgbspic"
    objs_gbsxmms="$objs_gbsxmms libgbspic.a"
fi

### END IF

objs="$objs $objs_gbsplay $objs_gbsinfo"
dsts="$dsts gbsplay gbsinfo"

if [ $build_xmmsplugin = yes ] ; then
    objs="$objs $objs_gbsxmms"
    dsts="$dsts gbsxmms.so"
fi





if [ $have_xgettext = yes ] ; then
    pos=po/*.po
    mos=$(echo $pos | sed 's/\.po/.mo/g')
    dsts="$dsts $mos"
fi

(
    while read var; do
	eval "echo \"$var=\\\"\$$var\\\"\""
    done << __EOF__
objs
dsts
docs
examples
contribs
mans
mans_1
mans_5
mans_src
SPLINT
SPLINTFLAGS
AR
BUILDCC
GBSCFLAGS
GBSLDFLAGS
GBSPLAYLDFLAGS
gbsplaybin
gbsinfobin
objs_gbsplay
objs_gbsinfo
objs_libgbs
pos
mos
DISTDIR
VERSION
EXTRA_ALL
TEST_TARGETS
TESTOPTS
EXTRA_INSTALL
bindir
man1dir
man5dir
docdir
exampledir
localedir
contribdir
xmmsdir
EXTRA_UNINSTALL
__EOF__
    
) > config.sh

cat config.h config.sed config.sh configure | redo-stamp
