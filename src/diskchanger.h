/* diskchanger.h
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
 */
#ifndef DISKCHANGER_H_
#define DISKCHANGER_H_

#include "vconf.h"
#include "changerstate.h"

class DiskChanger
{
public:
   DiskChanger();
   virtual ~DiskChanger();
   int Initialize(VchangerConfig &conf);
   int LoadDrive(int drv, int slot);
   int UnloadDrive(int drv);
   int GetDriveSlot(int drv);
   int GetSlotDrive(int slot);
   int CreateMagazine(int mndx, int magnum = -1);
   int GetLastError(char *msg, size_t msg_size);
   void Unlock();
protected:
   bool Lock(long timeout = 1000);
   void internalUnlock();
   void SetDefaults();
   int GetDriveSymlinkDir(int drv, char *lnkdir, size_t lnkdir_size);
   int DoLoad(int magndx, int drv, VolumeLabel &lab);
   int DoUnload(int magndx, int drive, VolumeLabel &lab);
   int SetDriveSymlink(int drv, const char *mountpoint);
   int RemoveDriveSymlink(int drv);
   int WriteMagazineIndex(const char *mountpt, int magnum);
   int ReadMagazineLoaded(const char *mountpt, int drive, VolumeLabel &lab);
   int WriteMagazineLoaded(const char *mountpt, int drive, VolumeLabel &lab);
   int ReadNextMagazineNumber();
   int WriteNextMagazineNumber(int magnum);
   int MagNum2MagBay(int mganum);
   int MagSlot2VirtualSlot(int magnum, int magslot);
   int VirtualSlot2MagSlot(int slot, int &mag_ndx);
   int VirtualSlot2VolumeLabel(int slot, VolumeLabel &lab);
   int VolumeLabel2VirtualSlot(const VolumeLabel &lab);
public:
   char changer_name[64];
   char work_dir[PATH_MAX];
   MagazineState magazine[MAX_MAGAZINE_BAYS];
   DriveState drive[MAX_DRIVES];
//   int32_t mag_number[MAX_MAGAZINES];
//   int32_t loaded[MAX_MAGAZINES];
   int32_t magazine_bays;
   int32_t virtual_drives;
   int32_t slots_per_mag;
   int32_t slots;
protected:
   FILE *changer_lock;
   int lasterr;
   char lasterrmsg[4096];
};

#endif /*DISKCHANGER_H_*/
