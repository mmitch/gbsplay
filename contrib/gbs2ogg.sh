#!/bin/sh
# $Id: gbs2ogg.sh,v 1.1 2003/10/11 20:18:49 mitch Exp $
#
# Automatically convert all subsongs from .gbs file to .ogg files.
# 
# 2003 (C) by Christian Garbs <mitch@cgarbs.de>
#
# ToDo:
# - ogg-Tags setzen

FILENAME=$1

ENCODER="oggenc -q6 -r"
PLAY=206
FADE=3

if [ -z $1 ] ; then
    echo "usage:  gbs2ogg.sh <gbs-file>"
    exit 1
fi

FILEBASE=`basename "$FILENAME"`
FILEBASE=`echo "$FILEBASE"|sed 's/.gbs$//'`

# get subsong count
SUBSONGS=`gbsinfo $FILENAME | grep ^Subsongs: | sed -e 's/^Subsongs: //' -e 's/\\.$//'`

for SUBSONG in `seq 1 $SUBSONGS`; do
    printf "== converting song %02d/%02d:\n" $SUBSONG $SUBSONGS
    gbsplay -s -f $FADE -t $PLAY "$FILENAME" $SUBSONG $SUBSONG | $ENCODER - > `printf "%s-%02d.ogg" "$FILEBASE" $SUBSONG`
done
