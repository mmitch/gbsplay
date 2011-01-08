SOURCEFILES=../*.c
redo-ifchange $SOURCEFILES
xgettext -k_ -kN_ --language c $SOURCEFILES -o - | msgen -o $3 -
