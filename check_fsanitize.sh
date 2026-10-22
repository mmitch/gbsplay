#!/bin/bash
ERRORLOG=sanitize-error.log

set -e

echo
echo '==== configue with -fsanitize=address'
./configure "${@}" --enable-verbosebuild --enable-debug CFLAGS="-fsanitize=address" LDFLAGS="-fsanitize=address"

echo
echo '==== run build'
make

echo
echo '==== check bad files'

rm -f $ERRORLOG
for FILE in badfiles/*.gb badfiles/*.gbr badfiles/*.gbs badfiles/*.vgm; do
    [ -r "$FILE" ] || continue
    echo
    echo "=== check playback of bad file $FILE:"
    echo
    set +e
    ASAN_OPTIONS=exitcode=99 ./gbsplay -o null "$FILE" -q 1 1 2>&1
    RC=$?
    set -e
    echo
    case $RC in
	99)
           ERR="Sanitizer detected error with file $FILE"
           echo "$ERR" >> $ERRORLOG
           echo "== $ERR"
	   ;;

	0) 
            echo "== no error"
	    ;;

	*)
            : RC=1 is expected for parse errors from broken files
            echo "== got error $RC, but that's ok"
	    ;;
    esac
    echo
done

if [[ -e $ERRORLOG ]]; then
    echo "=== ERRORS ENCOUNTERED"
    echo
    exit 1
fi

echo "=== ALL OK"
echo
