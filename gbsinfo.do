redo-ifchange config
. ./config.sh
redo-ifchange $objs_gbsinfo
## TODO: here we be ignoring $gbsinfobin and won't work under Cygwin!
# $BUILDCC -o $gbsinfobin $objs_gbsinfo $GBSLDFLAGS
$BUILDCC -o $3 $objs_gbsinfo $GBSLDFLAGS
