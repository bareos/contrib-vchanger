#!/bin/sh
name=$1
devdir=/dev/disk/by-label
n=1
while [ -e ${devdir}/${name}.$n ]; do
    n=`expr ${n} + 1`
done
echo ID_FS_LABEL_SAFE=${name}.${n}
exit 0
