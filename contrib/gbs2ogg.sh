#!/bin/sh
# $Id: gbs2ogg.sh,v 1.3 2003/12/19 21:46:10 mitch Exp $
#
# Automatically convert all subsongs from .gbs file to .ogg files.
# 
# 2003 (C) by Christian Garbs <mitch@cgarbs.de>
#

FILENAME=$1

ENCODER="oggenc -q6 -r"
PLAY=206
FADE=3

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
	gbsplay -s -f $FADE -t $PLAY "$FILENAME" $SUBSONG $SUBSONG \
	    | $ENCODER -N $SUBSONG -t "$TITLE" -a "$AUTHOR" -c "copyright=$COPYRIGHT" -G "Gameboy music" - \
	    > `printf "%s-%02d.ogg" "$FILEBASE" $SUBSONG`
    done
)
