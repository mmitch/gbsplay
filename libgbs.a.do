redo-ifchange config
. ./config.sh
redo-ifchange $objs_libgbs ar
./ar $3 $objs_libgbs
