#!/bin/sh

set -eu

in="$1"
out="$2"
fmt="$3"

exec 1>"$out"

if [ "$fmt" = "ver" ]; then
	echo "{"
	echo "  global:"
else
	echo "EXPORTS"
fi

while read name; do
	if [ "$fmt" = "ver" ]; then
		echo "    ${name};"
	else
		echo "	${name}"
	fi
done < "$in"

if [ "$fmt" = "ver" ]; then
	echo "  local:"
	echo "    *;"
	echo "};"
fi
