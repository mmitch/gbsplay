#!/bin/sh
DIR=`dirname $1`

if [ "$DIR" = "" -o "$DIR" = "." ]; then
	DIR=""
else
	DIR="$DIR/"
fi

$CC -M $CFLAGS "$1" |
	sed -n -e "
		s@^\\(.*\\)\\.o:@$DIR\\1.d $DIR\\1.o: Makefile@
		s@/usr/[^ ]*\\.h@@g
		t foo
		:foo
		s@^ *\\\\\$@@
		t
		p
		"
