/* changerstate.h
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
#ifndef CHANGERSTATE_H_
#define CHANGERSTATE_H_

#include <vector>
#include "tstring.h"
#include "errhandler.h"

class MagazineSlot
{
public:
   MagazineSlot() : mag_bay(-1), mag_slot(-1) {}
   MagazineSlot(const MagazineSlot &b);
	virtual ~MagazineSlot() {}
	MagazineSlot& operator=(const MagazineSlot &b);
   bool operator==(const MagazineSlot &b);
   bool operator!=(const MagazineSlot &b);
   void clear();
   inline bool empty() { return label.empty(); }
   inline bool empty() const { return label.empty(); }
   inline bool operator==(const char *lab) { return (label == lab); }
   inline bool operator!=(const char *lab) { return (label != lab); }
public:
	int mag_bay;
	int mag_slot;
	tString label;
};

typedef std::vector<MagazineSlot> MagazineSlotArray;

class MagazineState
{
public:
   MagazineState() : mag_bay(-1), num_slots(0), start_slot(0), prev_num_slots(0), prev_start_slot(0) {}
	MagazineState(const MagazineState &b);
	virtual ~MagazineState() {}
	MagazineState& operator=(const MagazineState &b);
	void clear();
   int save();
	int restore();
	int Mount();
	void SetBay(int bay, const char *dev);
	inline void SetBay(int bay, const tString &dev) { SetBay(bay, dev.c_str()); }
   tString GetVolumePath(int mag_slot);
   const char* GetVolumePath(tString &path, int slot);
   const char* GetVolumeLabel(int mag_slot) const;
   int GetVolumeSlot(const char *fname);
   inline int GetVolumeSlot(const tString &fname) { return GetVolumeSlot(fname.c_str()); }
	int CreateVolume(const char *vol_label = "");
	inline int CreateVolume(const tString &labl) { return CreateVolume(labl.c_str()); }
   inline bool empty() { return mountpoint.empty(); }
   inline bool empty() const { return mountpoint.empty(); }
protected:
	int ReadMagazineIndex();
	int UpdateMagazineFormat();
public:
	int mag_bay;
	int num_slots;
	int start_slot;
	int prev_num_slots;
	int prev_start_slot;
	tString mag_dev;
	tString mountpoint;
	MagazineSlotArray mslot;
   ErrorHandler verr;
};

typedef std::vector<MagazineState> MagazineStateArray;

class VirtualSlot
{
public:
   VirtualSlot() : vs(0), drv(-1), mag_bay(-1), mag_slot(-1) {}
   VirtualSlot(const VirtualSlot &b);
   virtual ~VirtualSlot() {}
   VirtualSlot& operator=(const VirtualSlot &b);
   void clear();
   bool empty() { return mag_bay < 0; }
   bool empty() const { return mag_bay < 0; }
public:
   int vs;
   int drv;
   int mag_bay;
   int mag_slot;
};

typedef std::vector<VirtualSlot> VirtualSlotArray;

class DynamicConfig
{
public:
   DynamicConfig() : max_slot(0) {}
   void save();
   void restore();
public:
   int max_slot;
};

class DriveState
{
public:
   DriveState() : drv(-1), vs(-1) {}
   DriveState(const DriveState &b);
   virtual ~DriveState() {}
   DriveState& operator=(const DriveState &b);
   void clear();
   inline bool empty() { return vs < 0; }
   inline bool empty() const { return vs < 0; }
public:
   int drv;
   int vs;
};

typedef std::vector<DriveState> DriveStateArray;

#endif /*CHANGERSTATE_H_*/
