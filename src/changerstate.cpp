/* changerstate.cpp
 *
 *  This file is part of vchanger by Josh Fisher.
 *
 *  vchanger copyright (C) 2008-2009 Josh Fisher
 *
 *  vchanger is free software.
 *  You may redistribute it and/or modify it under the terms of the
 *  GNU General Public License version 2, as published by the Free
 *  Software Foundation.
 *
 *  vchanger is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with vchanger.  See the file "COPYING".  If not,
 *  write to:  The Free Software Foundation, Inc.,
 *             59 Temple Place - Suite 330,
 *             Boston,  MA  02111-1307, USA.
 * 
 *  Provides a class to track state of virtual drives using files in
 *  the vchanger state directory.
*/

#include "vchanger.h"
#include "util.h"
#include "changerstate.h"
#include <sys/stat.h>


///////////////////////////////////////////////////
//  Class DriveState
///////////////////////////////////////////////////

DriveState::DriveState()
{
   drive = -1;
   mag_number = -1;
   mag_slot = -1;
   statedir[0] = 0;
   chgr_name[0] = 0;
}

DriveState::~DriveState()
{
}

void DriveState::unset()
{
   if (drive < 0) {
      return;
   }
   mag_number = -1;
   mag_slot = -1;
   chgr_name[0] = 0;
}


int DriveState::set(const char *chngr, int magnum, int magslot)
{
   char vname[128];

   if (drive < 0 || !BuildVolumeName(vname, sizeof(vname), chngr, magnum, magslot)) {
      return -1;
   }
   return set(vname);
}


int DriveState::set(const char *volname)
{
   char chngr[128];
   int magnum, magslot;
   if (drive < 0 || ParseVolumeName(volname, chngr, sizeof(chngr), magnum, magslot)) {
      return -1;
   }
   mag_number = magnum;
   mag_slot = magslot;
   strncpy(chgr_name, chngr, sizeof(chgr_name));
   return 0;
}


int DriveState::save()
{
   FILE *FS;
   char sname[PATH_MAX], vname[256];
   if (drive < 0) {
      return -1;
   }
   snprintf(sname, sizeof(sname), "%s%sstate%d", statedir, DIR_DELIM, drive);
   BuildVolumeName(vname, sizeof(vname), chgr_name, mag_number, mag_slot);
   FS = fopen(sname, "w");
   if (!FS) {
      return -1;
   }
   if (fprintf(FS, "%s", vname) < 0) {
      fclose(FS);
      unlink(sname);
      return -1;
   }
   fclose(FS);
   return 0;
}


void DriveState::clear()
{
   char sname[PATH_MAX];

   if (drive < 0) {
      return;
   }
   snprintf(sname, sizeof(sname), "%s%sstate%d", statedir, DIR_DELIM, drive);
   unlink(sname);
   mag_number = -1;
   mag_slot = -1;
   chgr_name[0] = 0;
}


int DriveState::restore()
{
   char sname[PATH_MAX], vname[256], chngr[128];
   int magnum, magslot;
   FILE *FS;

   if (drive < 0) {
      return -1;
   }
   mag_number = -1;
   mag_slot = -1;
   chgr_name[0] = 0;
   snprintf(sname, sizeof(sname), "%s%sstate%d", statedir, DIR_DELIM, drive);
   FS = fopen(sname, "r");
   if (!FS) {
      return -1;
   }
   if (!fgets(vname, sizeof(vname), FS)) {
      fclose(FS);
      unlink(sname);
      return -1;
   }
   fclose(FS);
   if (ParseVolumeName(vname, chngr, sizeof(chngr), magnum, magslot)) {
      unlink(sname);
      return -1;
   }
   /* restore state info for this drive */
   mag_number = magnum;
   mag_slot = magslot;
   strncpy(chgr_name, chngr, sizeof(chgr_name));
   return 0;
}


///////////////////////////////////////////////////
//  Class MagazineState
///////////////////////////////////////////////////

MagazineState::MagazineState()
{
   mag_number = -1;
   memset(mountpoint, 0, sizeof(mountpoint));
}

MagazineState::~MagazineState()
{
}
