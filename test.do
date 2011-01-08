redo-ifchange gbsplay

MD5=`LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH ./gbsplay -E b -o stdout $TESTOPTS examples/nightmode.gbs 1 < /dev/null | md5sum | cut -f1 -d\ `
EXPECT="5269fdada196a6b67d947428ea3ca934"

if [ "$MD5" = "$EXPECT" ]; then
    echo "Bigendian output ok" >&2
else
    echo "Bigendian output failed" >&2
    echo "  Expected: $EXPECT" >&2
    echo "  Got:      $MD5" >&2
#    exit 1 TODO
fi

MD5=`LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH ./gbsplay -E l -o stdout $TESTOPTS examples/nightmode.gbs 1 < /dev/null | md5sum | cut -f1 -d\ `
EXPECT="3c005a70135621d8eb3e0dc20982eba8"

if [ "$MD5" = "$EXPECT" ]; then
    echo "Littleendian output ok" >&2
else
    echo "Littleendian output failed" >&2
    echo "  Expected: $EXPECT" >&2
    echo "  Got:      $MD5" >&2
#    exit 1 TODO
fi

