#!/bin/sh
#
#  vchanger-launch-umount.sh ( vchanger v.1.0.1 ) 2015-06-03
#
#  This script is used to run the vchanger-umount-uuid.sh script in
#  another [background] process launched by the at command in order
#  to prevent delays when invoked by a udev rule. 
#
VCHANGER_UMOUNT=/usr/libexec/vchanger/vchanger-umount-uuid.sh
{
$VCHANGER_UMOUNT $1
} &
