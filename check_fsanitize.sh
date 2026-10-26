#!/bin/bash
set -e

SUBDIR=badfiles
OUTPUTLOG=sanitize-output.log
ERRORLOG=sanitize-error.log

write_error() {
    local error="$*"
    echo "$error" >> $ERRORLOG
    echo "== $error"
}

echo
echo '==== configue with -fsanitize=address'
./configure "${@}" --enable-verbosebuild --enable-debug CFLAGS="-fsanitize=address" LDFLAGS="-fsanitize=address"

echo
echo '==== run build'
make

echo
echo '==== check bad files'

rm -f $ERRORLOG
for FILE in "$SUBDIR"/*.{gb,gbr,gbs,vgm}; do
    [ -r "$FILE" ] || continue
    echo
    echo "=== check playback of bad file $FILE:"
    echo
    set +e
    ASAN_OPTIONS=exitcode=99 ./gbsplay -o null "$FILE" -q 1 1 2>&1 | tee $OUTPUTLOG
    RC=$?
    set -e
    echo
    case $RC in
        99)
            write_error "Sanitizer detected error with file $FILE"
            ;;

        0)
            echo "== no error"
            ;;

        *)
            echo "== got error $RC, but that's ok, we're triggering errors"
            ;;
    esac
    if EXPECTED=$(grep -A3 "${FILE#"$SUBDIR"/}" $SUBDIR/FILES.txt | grep -F "expected validation error"); then
        EXPECTED=${EXPECTED#*: }
        echo
        echo "looking for '$EXPECTED' in output"
        if grep -qF "$EXPECTED" $OUTPUTLOG; then
            echo "== ok, found"
        else
            write_error "'$EXPECTED' not found in output"
        fi
    fi
    echo
done

if [[ -e $ERRORLOG ]]; then
    echo "=== ERRORS ENCOUNTERED"
    echo
    exit 1
fi

echo "=== ALL OK"
echo
