redo-ifchange config
. ./config.sh
echo "$AR r \$@" > $3
chmod a+x $3
