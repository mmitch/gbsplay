#!/bin/sh
DIR=`dirname $1`
FILE="$1"
SUBMK=""

shift

EXTRADEP=" $*"

if [ "$DIR" = "" -o "$DIR" = "." ]; then
	DIR=""
else
	DIR="$DIR/"
fi
if [ -f "${DIR}subdir.mk" ]; then
	SUBMK=" ${DIR}subdir.mk"
fi

exec $CC -M $GBSCFLAGS "$FILE" |
	sed -n -e "
		s@^\\(.*\\)\\.o:@$DIR\\1.d $DIR\\1.o $DIR\\1.lo: depend.sh Makefile$SUBMK$EXTRADEP@
		s@/usr/[^	 ]*@@g
		t foo
		:foo
		s@^[	 ]*\\\\\$@@
		t
		p
		"
