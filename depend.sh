#!/bin/sh
DIR=`dirname $1`
case "$DIR" in
	"" | ".")
		${CC} -M "$@" | sed -e 's@^\(.*\)\.o:@\1.d \1.o: Makefile@'
		;;
	*)
		${CC} -M "$@" | sed -e "s@^\(.*\)\.o:@$DIR/\1.d $DIR/\1.o: Makefile@"
		;;
esac
