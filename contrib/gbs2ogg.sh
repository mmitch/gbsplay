#!/bin/sh
# $Id: gbs2ogg.sh,v 1.7 2003/12/28 19:28:59 mitch Exp $
#
# Automatically convert all subsongs from .gbs file to .ogg files.
# 
# 2003 (C) by Christian Garbs <mitch@cgarbs.de>
#

FILENAME=$1

RATE=44100
PLAY=150
FADE=6
GAP=0

if [ -z "$1" ]; then
    echo "usage:  gbs2ogg.sh <gbs-file> [subsong_seconds]"
    exit 1
fi

if [ "$2" ]; then
    PLAY=$2
fi

FILEBASE=`basename "$FILENAME"`
FILEBASE=`echo "$FILEBASE"|sed 's/.gbs$//'`

# get subsong count
# get song info
    gbsinfo "$FILENAME" | cut -c 17- | sed -e 's/^"//' -e 's/"$//' | (
    read GBSVERSION
    read TITLE
    read AUTHOR
    read COPYRIGHT
    read LOAD_ADDR
    read INIT_ADDR
    read PLAY_ADDR
    read STACK_PTR
    read FILE_SIZE
    read ROM_SIZE
    read SUBSONGS

    for SUBSONG in `seq 1 $SUBSONGS`; do
	printf "== converting song %02d/%02d:\n" $SUBSONG $SUBSONGS
	gbsplay -s -E l -r $RATE -g $GAP -f $FADE -t $PLAY "$FILENAME" $SUBSONG $SUBSONG \
	    | oggenc -q6 -r --raw-endianness 0 -B 16 -C 2 -R $RATE -N $SUBSONG -t "$TITLE" -a "$AUTHOR" -c "copyright=$COPYRIGHT" -c "COMMENT=Subsong $SUBSONG" -G "Gameboy music" - \
	    > "`printf "%s-%02d.ogg" "$FILEBASE" $SUBSONG`"
    done
)
