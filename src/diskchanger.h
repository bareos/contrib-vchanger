/* diskchanger.h
 *
 *  This file is part of vchanger by Josh Fisher.
 *
 *  vchanger copyright (C) 2008-2015 Josh Fisher
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
#include "errhandler.h"
#include "changerstate.h"

class DiskChanger
{
public:
   DiskChanger() : changer_lock(NULL), needs_update(false), needs_label(false)  {}
   virtual ~DiskChanger();
   int Initialize();
   int LoadDrive(int drv, int slot);
   int UnloadDrive(int drv);
   int CreateVolumes(int bay, int count, int start = -1, const char *label_prefix = "");
   int UpdateBacula();
   const char* GetVolumeLabel(int slot);
   const char* GetVolumePath(tString &fname, int slot);
   bool MagazineEmpty(int bay) const;
   bool SlotEmpty(int slot) const;
   bool DriveEmpty(int drv) const;
   int GetDriveSlot(int drv) const;
   int GetSlotDrive(int slot) const;
   int GetMagazineSlots(int mag) const;
   int GetMagazineStartSlot(int mag) const;
   const char* GetMagazineMountpoint(int mag) const;
   inline int NumDrives() { return (int)drive.size(); }
   inline int NumMagazines() { return (int)magazine.size(); }
   inline int NumSlots() { return (int)vslot.size() - 2; }
   inline int GetError() { return verr.GetError(); }
   inline const char* GetErrorMsg() const { return verr.GetErrorMsg(); }
   inline bool NeedsUpdate() const { return needs_update; }
   inline bool NeedsLabel() const { return needs_label; }
   int Lock(long timeout = 30);
   void Unlock();
protected:
   void InitializeMagazines();
   int FindEmptySlotRange(int count);
   int InitializeDrives();
   void InitializeVirtSlots();
   void SetMaxDrive(int n);
   int CreateDriveSymlink(int drv);
   int RemoveDriveSymlink(int drv);
   int SaveDriveState(int drv);
   int RestoreDriveState(int drv);
protected:
   FILE *changer_lock;
   bool needs_update;
   bool needs_label;
   ErrorHandler verr;
   DynamicConfig dconf;
   MagazineStateArray magazine;
   DriveStateArray drive;
   VirtualSlotArray vslot;
};

#endif /*DISKCHANGER_H_*/
