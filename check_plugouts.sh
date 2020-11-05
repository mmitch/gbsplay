#!/usr/bin/env bash
#
# Ensure that configure enabled the plugouts as it was expected to.
# This needs a bash with hashmap support but it is only used on the build machines
#
# 2020 (C) by Christian Garbs <mitch@cgarbs.de>
# Licensed under GNU GPL v1 or, at your option, any later version.

set -e

die()
{
    echo "$@" >&2
    exit 1
}

# these don't have dependencies and should always be built
declare -A expected_plugouts=( [iodumper]=1 [midi]=1 [altmidi]=1 [stdout]=1 )

# additional plugouts are given as commandline parameters
for arg in $*; do
    expected_plugouts[$arg]=1
done

# read configured plugouts
declare -A enabled_plugouts

while read -r prefixed_name _ state; do
    name=${prefixed_name:8}
    if [ "$state" = yes ]; then
	enabled_plugouts[$name]=1
    fi
done < <( grep ^plugout config.mk)

# check that all expected plugouts are present
for expected in "${!expected_plugouts[@]}"; do
    if [ "${enabled_plugouts[$expected]}" != 1 ]; then
	die "expected plugout <$expected> has not been enabled by configure"
    fi
done

# check that no unexpected plugouts are present
for enabled in "${!enabled_plugouts[@]}"; do
    if [ "${expected_plugouts[$enabled]}" != 1 ]; then
	die "unexpected plugout <$enabled> has been enabled by configure"
    fi
done
