redo-ifchange config
. ./config.sh
redo-ifchange $objs_gbsplay
## TODO: here we be ignoring $gbsplaybin and won't work under Cygwin!
# $BUILDCC -o $gbsplaybin $objs_gbsplay $GBSLDFLAGS $GBSPLAYLDFLAGS -lm
$BUILDCC -o $3 $objs_gbsplay $GBSLDFLAGS $GBSPLAYLDFLAGS -lm
