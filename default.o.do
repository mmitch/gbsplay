redo-ifchange $1.c compile
gcc -MD -MF $3.deps.tmp -c -o $3 $1.c
DEPS=$(sed -e "s/^$3://" -e 's/\\//g' <$3.deps.tmp)
rm -f $3.deps.tmp
redo-ifchange $DEPS
./compile $1 $2 $3
