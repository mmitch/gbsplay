redo-ifchange configure
./configure
cat >> config.sh << EOF
MANPAGES="gbsplay.1 gbsinfo.1 gbsplayrc.5"
EOF
cat config.h config.mk config.sed config.sh configure | redo-stamp
