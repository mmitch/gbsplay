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
    echo "error"
    echo "$@" >&2
    exit 1
}

key_contains()
{
    local key="$1" searched="$2" 

    local -a values
    read -a values -r < <(grep "^$key := " config.mk)

    local value
    # :2 skips the first two array entries ($key and ":=")
    for value in "${values[@]:2}"; do
	if [ "$value" = "$searched" ]; then
	    return 0
	fi
    done
    return 1
}

expect_to_contain()
{
    local key="$1" expected="$2"
    
    echo -n "check <$key> to contain <$expected> .. "
    if ! key_contains "$key" "$expected"; then
	die "expected value <$expected> not found in key <$key>"
    fi
    echo "ok"
}

expect_to_not_contain()
{
    local key="$1" unexpected="$2"

    echo -n "check <$key> to not contain <$unexpected> .. "
    if key_contains "$key" "$unexpected"; then
	die "unexpected value <$unexpected> found in key <$key>"
    fi
    echo "ok"
}

# default configuration when all dependencies are available
i18n=yes
zlib=yes
harden=yes
sharedlib=no
xgbsplay=no

# parse build configuration
for flag in "$@"; do
    case "$flag" in
	"")
	;;
	--disable-i18n)
	    i18n=no
	    ;;
	--disable-zlib)
	    zlib=no
	    ;;
	--disable-hardening)
	    harden=no
	    ;;
	--enable-sharedlibgbs)
	    sharedlib=yes
	    ;;
	--with-xgbsplay)
	    xgbsplay=yes
	    ;;
	*)
	    die "unhandled flag <$flag>"
	    ;;
    esac
done

# dump status
echo "expected configuration: i18n=$i18n zlib=$zlib harden=$harden sharedlib=$sharedlib"
echo "checking config.mk"

# test configuration
expect_to_contain use_i18n "$i18n"

if [ "$zlib" = yes ]; then
    expect_to_contain EXTRA_LDFLAGS "-lz"
else
    expect_to_not_contain EXTRA_LDFLAGS "-lz"
fi

if [ "$harden" = yes ]; then
    expect_to_contain EXTRA_CFLAGS "-fstack-protector-strong"
    expect_to_contain EXTRA_LDFLAGS "-fstack-protector-strong"
else
    expect_to_not_contain EXTRA_CFLAGS "-fstack-protector"
    expect_to_not_contain EXTRA_CFLAGS "-fstack-protector-strong"
    expect_to_not_contain EXTRA_CFLAGS "-fstack-clash-protection"
    expect_to_not_contain EXTRA_LDFLAGS "-fstack-protector"
    expect_to_not_contain EXTRA_LDFLAGS "-fstack-protector-strong"
    expect_to_not_contain EXTRA_LDFLAGS "-fstack-clash-protection"
fi

expect_to_contain use_sharedlibgbs "$sharedlib"

if [ "$xgbsplay" = yes ]; then
    expect_to_contain EXTRA_INSTALL "xgbsplay"
    expect_to_contain EXTRA_UNINSTALL "xgbsplay"
else
    expect_to_not_contain EXTRA_INSTALL "xgbsplay"
    expect_to_not_contain EXTRA_UNINSTALL "xgbsplay"
fi

# ok
echo "ok"
