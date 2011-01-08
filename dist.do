redo-ifchange config
. ./config.sh
redo distclean #always!
install -d ./$DISTDIR
sed "s/^VERSION=.*/VERSION=$VERSION/" < configure > ./$DISTDIR/configure
chmod 755 ./$DISTDIR/configure
install -m 644 Makefile ./$DISTDIR/
install -m 644 *.do ./$DISTDIR/
install -m 644 *.c ./$DISTDIR/
install -m 644 *.h ./$DISTDIR/
install -m 644 *.ver ./$DISTDIR/
install -m 644 $mans_src ./$DISTDIR/
install -m 644 $docs INSTALL CODINGSTYLE ./$DISTDIR/
install -d ./$DISTDIR/examples
install -m 644 $examples ./$DISTDIR/examples
install -d ./$DISTDIR/contrib
install -m 644 $contribs ./$DISTDIR/contrib
install -d ./$DISTDIR/po
install -m 644 po/*.po ./$DISTDIR/po
install -m 644 po/*.do ./$DISTDIR/po
tar -czf ../$DISTDIR.tar.gz $DISTDIR/ 
rm -rf ./$DISTDIR
