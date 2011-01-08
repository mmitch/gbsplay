redo-ifchange config.sed
echo "sed -f config.sed \$1.in\$2 > \$3" > $3
chmod a+x $3
