#!/bin/sh
#
#  vchanger-launch-mount.sh ( vchanger v.1.0.1 ) 2015-06-03
#
#  This script is used to run the vchanger-mount-uuid.sh script in
#  another [background] process launched by the at command in order
#  to prevent delays when invoked by a udev rule. 
#
VCHANGER_MOUNT=/usr/libexec/vchanger/vchanger-mount-uuid.sh
{
$VCHANGER_MOUNT $1
} &
