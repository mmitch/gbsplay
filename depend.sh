#!/bin/sh
DIR=`dirname $1`
SUBMK=""

if [ "$DIR" = "" -o "$DIR" = "." ]; then
	DIR=""
else
	DIR="$DIR/"
	SUBMK=" ${DIR}subdir.mk"
fi

$CC -M $CFLAGS "$1" |
	sed -n -e "
		s@^\\(.*\\)\\.o:@$DIR\\1.d $DIR\\1.o: depend.sh Makefile$SUBMK@
		s@/usr/[^ ]*\\.h@@g
		t foo
		:foo
		s@^ *\\\\\$@@
		t
		p
		"
