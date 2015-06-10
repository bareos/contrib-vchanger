#!/bin/bash
#
#  vchanger-mount-uuid.sh ( vchanger v.1.0.1 ) 2015-06-03
#
#  This script is used to mount the filesystem having the
#  UUID specified in parameter 1 at a fixed path. The path
#  will be a subdirectory of the path given by the MOUNT_DIR
#  environment variable, where the UUID is used as the
#  subdirectory name. The MOUNT_DIR variable is sourced from
#  /etc/default/vchanger or /etc/sysconfig/vchanger. Also,
#  the MOUNT_OPTIONS variable may be set to specify additional
#  options to pass through to the mount command.
#
#  Generally, this script is designed to be invoked by a
#  launcher script, vchanger-launch-mount.sh, that is itself
#  launched directly by udev in response to a hotplug event.
#  This is useful for headless systems that do not have an
#  automount capability such as Gnome Nautilus, but have
#  non-root daemons that require plug-n-play attachment of
#  external disk drives.
#

MOUNT_DIR="/mnt"
MOUNT_OPTIONS=
[ -f /etc/sysconfig/vchanger ] && . /etc/sysconfig/vchanger
[ -f /etc/default/vchanger ] && . /etc/default/vchanger
[ -d "$MOUNT_DIR" ] || exit 0  # mount directory does not exist

#  Parameter 1 specifies the UUID of the magazine
#  to be mounted
uuid=${1,,}
[ -n "$uuid" ] || exit 0  # ignore invalid param

#
#  Return mountpoint of filesystem having UUID passed in param 1
#  or else empty if UUID not found in fstab
#
function check_fstab {
  local uuid=${1,,}
  cat /etc/fstab | grep "^UUID=" | while read lin; do
    line=`echo $lin | sed -e "s/[[:space:]]\+/ /g"`
    tmp=`echo $line | cut -s -d ' ' -f 1 | cut -s -d '=' -f 2`
    fu=${tmp,,}
    if [ "z$fu" == "z$uuid" ]; then
      echo $line | cut -s -d ' ' -f 2
      return 0
    fi
  done
}

#  Search all autochanger configuration files for a magazine
#  definition matching the UUID in parameter 1
for cf in `ls -1 /etc/vchanger/*.conf` ; do
  for tmp in `cat ${cf} | grep -v "^#" | tr -d " \t" | grep -i "^magazine="` ; do
    tmp=`echo $tmp | grep -i "^magazine=uuid:"`
    if [ -n "$tmp" ] ; then
      tmp=`echo $tmp | cut -d ':' -f 2 | tr -d ' \t\r\n'`
      tmp=${tmp,,}
      if [ "z$uuid" == "z$tmp" ] ; then
        # param 1 UUID matches a magazine filesystem
        mdir=$(check_fstab $uuid)
        if [ -n "$mdir" ]; then
          # filesystem has UUID entry in fstab, so use it
          if [ ! -d $mdir ]; then
            mkdir -p $mdir &>/dev/null
            [ -d $mdir ] || exit 0  # cannot create mountpoint dir
            chmod 0750 $mdir
          fi
          mount $mdir
          [ $? -eq 0 ] || exit 0
          /usr/bin/vchanger $cf refresh
          exit 0
        fi
        # Mount under configured MOUNT_DIR
        if [ ! -d $MOUNT_DIR/$uuid ]; then
          mkdir -p $MOUNT_DIR/$uuid &>/dev/null
          [ -d $MOUNT_DIR/$uuid ] || exit 0  # cannot create mountpoint dir
          chmod 0750 $MOUNT_DIR/$uuid
        fi
        echo checking dev
        [ -e /dev/disk/by-uuid/$uuid ] || exit 0  # udev has no by-uuid symlinks
        mount $MOUNT_OPTIONS /dev/disk/by-uuid/$uuid $MOUNT_DIR/$uuid &>/dev/null
        if [ $? -eq 0 ] ; then
		  # On successful mount, cause update slots to be issued in bconsole
          /usr/bin/vchanger $cf refresh
		fi
        exit 0
      fi
    fi
  done
done
exit 0
            