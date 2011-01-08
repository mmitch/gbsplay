redo-ifchange config
. ./config.sh

# remove files

rm -f $bindir/$gbsplaybin $bindir/$gbsinfobin
for i in $mans_1; do
    rm -f $man1dir/$i
done
for i in $mans_5; do
    rm -f $man5dir/$i
done
for i in $mos; do
    base=`basename $i`
    rm -f $localedir/${base%.mo}/LC_MESSAGES/gbsplay.mo
    rmdir -p $localedir/${base%.mo}/LC_MESSAGES || true
done

# remove exclusive directories
# (and recreate empty for next step)

rm -rf $exampledir
rm -rf $docdir
mkdir -p $exampledir
mkdir -p $docdir

# remove shared directories if possible

rmdir -p $bindir || true
rmdir -p $man1dir || true
rmdir -p $man5dir || true
rmdir -p $exampledir || true
rmdir -p $docdir || true
