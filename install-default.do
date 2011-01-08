redo-ifchange config
. ./config.sh
install -d $bindir
install -d $man1dir
install -d $man5dir
install -d $docdir
install -d $exampledir
install -m 755 $gbsplaybin $gbsinfobin $bindir
install -m 644 gbsplay.1 gbsinfo.1 $man1dir
install -m 644 gbsplayrc.5 $man5dir
install -m 644 $docs $docdir
install -m 644 $examples $exampledir
for i in $mos; do
    base=`basename $i`
    install -d $localedir/${base%.mo}/LC_MESSAGES
    install -m 644 $i $localedir/${base%.mo}/LC_MESSAGES/gbsplay.mo
done

