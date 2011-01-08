redo-ifchange config
. ./config.sh
find . -regex ".*\.\([aos]\|lo\|mo\|pot\|so\(\.[0-9]\)?\)" -exec rm -f "{}" \;
find . -name "*~" -exec rm -f "{}" \;
rm -f $mans
rm -f $gbsplaybin $gbsinfobin
