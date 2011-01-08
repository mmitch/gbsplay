redo-ifchange config
. ./config.sh
redo clean #always!
find . -regex ".*\.d" -exec rm -f "{}" \;
rm -f ./config.mk ./config.h ./config.err ./config.sed ./config.sh ./config
# remove generated redo stuff
rm -rf .redo/
rm -f ar compile manpage 
